/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "package_manager.h"

#include "configure.h"
#include "linglong/api/types/helper.h"
#include "linglong/api/types/v1/Generators.hpp"
#include "linglong/api/types/v1/PackageInfoV2.hpp"
#include "linglong/api/types/v1/PackageManager1PruneResult.hpp"
#include "linglong/api/types/v1/Repo.hpp"
#include "linglong/api/types/v1/State.hpp"
#include "linglong/common/dir.h"
#include "linglong/common/strings.h"
#include "linglong/extension/extension.h"
#include "linglong/package/layer_file.h"
#include "linglong/package/layer_packager.h"
#include "linglong/package/reference.h"
#include "linglong/package/uab_file.h"
#include "linglong/package_manager/package_task.h"
#include "linglong/package_manager/package_update.h"
#include "linglong/package_manager/ref_installation.h"
#include "linglong/package_manager/uab_installation.h"
#include "linglong/repo/config.h"
#include "linglong/repo/ostree_repo.h"
#include "linglong/runtime/run_context.h"
#include "linglong/utils/error/error.h"
#include "linglong/utils/finally/finally.h"
#include "linglong/utils/hooks.h"
#include "linglong/utils/log/log.h"
#include "linglong/utils/packageinfo_handler.h"
#include "linglong/utils/serialize/json.h"
#include "linglong/utils/transaction.h"
#include "ocppi/runtime/RunOption.hpp"

#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusUnixFileDescriptor>
#include <QDebug>
#include <QEventLoop>
#include <QJsonArray>
#include <QMetaObject>
#include <QTimer>
#include <QUuid>

#include <algorithm>
#include <cstdint>
#include <utility>

#include <fcntl.h>

namespace linglong::service {

namespace {

constexpr auto repoLockPath = "/run/linglong/lock";

template <typename T>
QVariantMap toDBusReply(const utils::error::Result<T> &x, std::string type = "display") noexcept
{
    Q_ASSERT(!x.has_value());

    return utils::serialize::toQVariantMap(
      api::types::v1::CommonResult{ .code = x.error().code(),       // NOLINT
                                    .message = x.error().message(), // NOLINT
                                    .type = std::move(type) });
}

QVariantMap toDBusReply(utils::error::ErrorCode code,
                        const std::string &message,
                        const std::string &type = "display") noexcept
{
    return utils::serialize::toQVariantMap(
      api::types::v1::CommonResult{ .code = static_cast<int>(code), // NOLINT
                                    .message = message,             // NOLINT
                                    .type = type });
}

} // namespace

PackageManager::PackageManager(linglong::repo::OSTreeRepo &repo,
                               linglong::runtime::ContainerBuilder &containerBuilder,
                               QObject *parent)
    : QObject(parent)
    , repo(repo)
    , tasks(this)
    , m_search_queue(this)
    , m_generator_queue(this)
    , containerBuilder(containerBuilder)
{
    // tasks and PackageManager are on a same thread, it's safe to getTask in slot
    QObject::connect(&tasks, &PackageTaskQueue::taskDone, this, [this](const QString &taskID) {
        auto ret = tasks.getTask(taskID.toStdString());
        if (!ret) {
            LogE("get task failed: {}", ret.error());
            return;
        }

        if (PackageTask *task = dynamic_cast<PackageTask *>(&(*ret).get()); task != nullptr) {
            Q_EMIT TaskRemoved(QDBusObjectPath{ task->taskObjectPath().c_str() });
        }
    });

    using namespace std::chrono_literals;
    auto deferredTimeOut = 3600s;
    auto *deferredTimeOutEnv = ::getenv("LINGLONG_DEFERRED_TIMEOUT");
    if (deferredTimeOutEnv != nullptr) {
        try {
            deferredTimeOut = std::stoi(deferredTimeOutEnv) * 1s;
        } catch (std::invalid_argument &e) {
            qWarning() << "failed to parse LINGLONG_DEFERRED_TIMEOUT[" << deferredTimeOutEnv
                       << "]:" << e.what();
        } catch (std::out_of_range &e) {
            qWarning() << "failed to parse LINGLONG_DEFERRED_TIMEOUT[" << deferredTimeOutEnv
                       << "]:" << e.what();
        }
    }

    qInfo().nospace() << "deferredTimeOut:" << deferredTimeOut.count() << "s";

    auto *timer = new QTimer(this);
    timer->setInterval(deferredTimeOut);
    connect(timer, &QTimer::timeout, [this, timer] {
        this->deferredUninstall();
        timer->start();
    });

    timer->start();
}

PackageManager::~PackageManager()
{
    auto ret = unlockRepo();
    if (!ret) {
        qCritical() << "failed to unlock repo:" << ret.error().message();
    }
}

utils::error::Result<bool> PackageManager::isRefBusy(const package::Reference &ref) noexcept
{
    LINGLONG_TRACE(fmt::format("check if ref[{}] is used by some apps", ref.toString()));

    auto ret = lockRepo();
    if (!ret) {
        return LINGLONG_ERR(
          QStringLiteral("failed to lock repo, underlying data will not be removed: %1")
            .arg(ret.error().message().c_str()));
    }

    auto unlock = utils::finally::finally([this] {
        auto ret = unlockRepo();
        if (!ret) {
            qCritical() << "failed to unlock repo:" << ret.error().message();
        }
    });

    auto running = getAllRunningContainers();
    if (!running) {
        return LINGLONG_ERR(QStringLiteral("failed to get running containers: %1")
                              .arg(running.error().message().c_str()));
    }
    auto &runningRef = *running;

    std::string refStr = ref.toString();
    return std::find_if(runningRef.cbegin(),
                        runningRef.cend(),
                        [&refStr](const api::types::v1::ContainerProcessStateInfo &info) {
                            if (info.app == refStr || info.base == refStr) {
                                return true;
                            }

                            if (info.runtime && *info.runtime == refStr) {
                                return true;
                            }

                            if (info.extensions) {
                                for (const auto &extension : *info.extensions) {
                                    if (extension == refStr) {
                                        return true;
                                    }
                                }
                            }

                            return false;
                        })
      != runningRef.cend();
}

utils::error::Result<std::vector<api::types::v1::ContainerProcessStateInfo>>
PackageManager::getAllRunningContainers() noexcept
{
    LINGLONG_TRACE("get all running containers");

    std::error_code ec;
    auto user_iterator = std::filesystem::directory_iterator{ "/run/linglong", ec };
    if (ec) {
        return LINGLONG_ERR(
          QStringLiteral("failed to list /run/linglong: %1").arg(ec.message().c_str()));
    }

    std::vector<api::types::v1::ContainerProcessStateInfo> result;
    for (const auto &entry : user_iterator) {
        if (!entry.is_directory()) {
            continue;
        }

        auto process_iterator = std::filesystem::directory_iterator{ entry.path(), ec };
        if (ec) {
            return LINGLONG_ERR(QStringLiteral("failed to list %1: %2")
                                  .arg(entry.path().c_str())
                                  .arg(ec.message().c_str()));
        }

        for (const auto &process_entry : process_iterator) {
            if (!process_entry.is_regular_file()) {
                continue;
            }

            auto pid = process_entry.path().filename().string();
            if (auto procDir = "/proc/" + pid; !std::filesystem::exists(procDir, ec)) {
                if (ec) {
                    return LINGLONG_ERR(QStringLiteral("failed to get state of %1: %2")
                                          .arg(procDir.c_str())
                                          .arg(ec.message().c_str()));
                }

                qInfo() << "ignore" << process_entry.path().c_str()
                        << ",because corrsponding process is not found.";
                continue;
            }

            auto content =
              utils::serialize::LoadJSONFile<api::types::v1::ContainerProcessStateInfo>(
                QString::fromStdString(process_entry.path().string()));
            if (!content) {
                return LINGLONG_ERR(QStringLiteral("failed to load info from %1: %2")
                                      .arg(process_entry.path().c_str())
                                      .arg(content.error().message().c_str()));
            }

            result.emplace_back(std::move(content).value());
        }
    }

    return result;
}

[[nodiscard]] utils::error::Result<void> PackageManager::lockRepo() noexcept
{
    LINGLONG_TRACE("lock whole repo")
    lockFd = ::open(repoLockPath, O_RDWR | O_CREAT, 0644);
    if (lockFd == -1) {
        return LINGLONG_ERR(QStringLiteral("failed to create lock file %1: %2")
                              .arg(repoLockPath)
                              .arg(::strerror(errno)));
    }

    struct flock locker{ .l_type = F_WRLCK, .l_whence = SEEK_SET, .l_start = 0, .l_len = 0 };

    if (::fcntl(lockFd, F_SETLK, &locker) == -1) {
        return LINGLONG_ERR(
          QStringLiteral("failed to lock %1: %2").arg(repoLockPath).arg(::strerror(errno)));
    }

    return LINGLONG_OK;
}

[[nodiscard]] utils::error::Result<void> PackageManager::unlockRepo() noexcept
{
    LINGLONG_TRACE("unlock whole repo")

    if (lockFd == -1) {
        return LINGLONG_OK;
    }

    struct flock unlocker{ .l_type = F_UNLCK, .l_whence = SEEK_SET, .l_start = 0, .l_len = 0 };

    if (::fcntl(lockFd, F_SETLK, &unlocker)) {
        return LINGLONG_ERR(
          QStringLiteral("failed to unlock %1: %2").arg(repoLockPath).arg(::strerror(errno)));
    }

    ::close(lockFd);
    lockFd = -1;

    return LINGLONG_OK;
}

utils::error::Result<void> PackageManager::applyApp(const package::Reference &reference) noexcept
{
    auto refStr = reference.toString();
    LINGLONG_TRACE(fmt::format("apply app {}", refStr));

    LogI("export new reference", refStr);
    this->repo.exportReference(reference);

    auto res = tryGenerateCache(reference);
    if (!res) {
        return LINGLONG_ERR(res);
    }

    return LINGLONG_OK;
}

utils::error::Result<void> PackageManager::unapplyApp(const package::Reference &reference) noexcept
{
    auto refStr = reference.toString();
    LINGLONG_TRACE(fmt::format("unapply app {}", refStr));

    auto removed = this->removeCache(reference);
    if (!removed) {
        LogW("Failed to remove old reference {} cache: {}", refStr, removed.error());
    }

    LogI("unexport old reference {}", refStr);
    this->repo.unexportReference(reference);

    return LINGLONG_OK;
}

utils::error::Result<void> PackageManager::switchAppVersion(const package::Reference &oldRef,
                                                            const package::Reference &newRef,
                                                            bool removeOldRef) noexcept
{
    LINGLONG_TRACE("remove old reference after install")
    LogI("switch app version from {} to {}", oldRef.toString(), newRef.toString());

    auto res = applyApp(newRef);
    if (!res) {
        return LINGLONG_ERR(res);
    }

    res = unapplyApp(oldRef);
    if (!res) {
        return LINGLONG_ERR(res);
    }

    if (removeOldRef) {
        auto res = tryUninstallRef(oldRef);
        if (!res) {
            return LINGLONG_ERR(res);
        }
    }

    auto mergeRet = this->repo.mergeModules();
    if (!mergeRet.has_value()) {
        LogE("merge modules failed: {}", mergeRet.error());
    }

    return LINGLONG_OK;
}

void PackageManager::deferredUninstall() noexcept
{
    if (auto ret = lockRepo(); !ret) {
        qCritical() << "failed to lock repo:" << ret.error().message();
        return;
    }
    auto unlock = utils::finally::finally([this] {
        auto ret = unlockRepo();
        if (!ret) {
            qCritical() << "failed to unlock repo:" << ret.error().message();
        }
    });

    // query layers which have been mark 'deleted'
    auto uninstalled = this->repo.listLocalBy(linglong::repo::repoCacheQuery{ .deleted = true });
    if (!uninstalled) {
        LogE("failed to list deleted layers: {}", uninstalled.error());
        return;
    }

    std::unordered_map<std::string, std::vector<api::types::v1::RepositoryCacheLayersItem>>
      uninstalledLayers;
    for (const auto &item : *uninstalled) {
        auto ref = package::Reference::fromPackageInfo(item.info);
        if (!ref) {
            LogE("underlying storage was broken: {}\n{}",
                 ref.error(),
                 nlohmann::json(item.info).dump());
            continue;
        }

        auto [node, isNew] =
          uninstalledLayers.try_emplace(ref->toString(),
                                        std::vector<api::types::v1::RepositoryCacheLayersItem>{});
        node->second.push_back(item);
    }

    if (uninstalledLayers.empty()) {
        return;
    }

    // retrieve running info
    auto running = getAllRunningContainers();
    if (!running) {
        LogE("failed to get all running containers: {}", running.error());
        return;
    }

    for (const auto &container : *running) {
        if (auto it = uninstalledLayers.find(container.app); it != uninstalledLayers.end()) {
            uninstalledLayers.erase(it);
        }
    }

    if (uninstalledLayers.empty()) {
        return;
    }

    for (const auto &[ref, items] : uninstalledLayers) {
        for (const auto &item : items) {
            // The app was already unapplied before being marked for lazy deletion.

            // Because there may be multiple items with the same reference, and one
            // of them could be marked as deleted. This scenario occurs when a layer
            // is marked as deleted, and then the same version of the UAB is installed
            // again. In this case, `UABInstallationAction` will overwrite the previous layer.
            auto ret = this->repo.remove(item);
            if (!ret) {
                LogE("failed to remove lazy deleted layer {}", ret.error());
                continue;
            }
        }
    }

    auto mergeRet = this->repo.mergeModules();
    if (!mergeRet) {
        LogE("failed to merge modules: {}", mergeRet.error());
    }
}

auto PackageManager::getConfiguration() const noexcept -> QVariantMap
{
    return utils::serialize::toQVariantMap(this->repo.getConfig());
}

void PackageManager::setConfiguration(const QVariantMap &parameters) noexcept
{
    LogI("set configuration for package manager");
    auto cfg = utils::serialize::fromQVariantMap<api::types::v1::RepoConfigV2>(parameters);
    if (!cfg) {
        sendErrorReply(QDBusError::InvalidArgs, QString::fromStdString(cfg.error().message()));
        return;
    }

    const auto &cfgRef = *cfg;
    const auto &curCfg = repo.getConfig();

    LogD("new config: {}", nlohmann::json(cfgRef).dump());
    LogD("cur config: {}", nlohmann::json(curCfg).dump());
    if (cfgRef == curCfg) {
        LogI("configuration not changed, ignore setting.");
        return;
    }
    if (const auto &defaultRepo = cfg->defaultRepo;
        std::find_if(cfg->repos.begin(),
                     cfg->repos.end(),
                     [&defaultRepo](const auto &repo) {
                         return repo.alias.value_or(repo.name) == defaultRepo;
                     })
        == cfg->repos.end()) {
        sendErrorReply(QDBusError::Failed,
                       "default repository is missing after updating configuration.");
        return;
    }

    auto result = this->repo.setConfig(*cfg);
    if (!result) {
        sendErrorReply(QDBusError::Failed, result.error().message().c_str());
    }
}

QVariantMap PackageManager::installFromLayer(const QDBusUnixFileDescriptor &fd,
                                             const api::types::v1::CommonOptions &options) noexcept
{
    auto layerFileRet = package::LayerFile::New(fd.fileDescriptor());
    if (!layerFileRet) {
        return toDBusReply(layerFileRet);
    }
    Q_ASSERT(*layerFileRet != nullptr);

    const auto &layerFile = *layerFileRet;
    auto realFile = layerFile->symLinkTarget();
    auto metaInfoRet = layerFile->metaInfo();
    if (!metaInfoRet) {
        return toDBusReply(metaInfoRet);
    }

    const auto &metaInfo = *metaInfoRet;
    auto packageInfoRet = utils::parsePackageInfo(metaInfo.info);
    if (!packageInfoRet) {
        return toDBusReply(packageInfoRet);
    }

    const auto &packageInfo = *packageInfoRet;

    auto architectureRet = package::Architecture::parse(packageInfo.arch[0]);
    if (!architectureRet) {
        return toDBusReply(architectureRet);
    }

    if (*architectureRet != package::Architecture::currentCPUArchitecture()) {
        return toDBusReply(utils::error::ErrorCode::Failed,
                           "app arch:" + architectureRet->toString()
                             + " not match host architecture");
    }

    auto versionRet = package::Version::parse(packageInfo.version);
    if (!versionRet) {
        return toDBusReply(versionRet);
    }

    auto packageRefRet = package::Reference::fromPackageInfo(packageInfo);
    if (!packageRefRet) {
        return toDBusReply(packageRefRet);
    }
    const auto &packageRef = *packageRefRet;
    api::types::v1::PackageManager1RequestInteractionAdditionalMessage additionalMessage;
    api::types::v1::InteractionMessageType msgType =
      api::types::v1::InteractionMessageType::Install;

    additionalMessage.remoteRef = packageRef.toString();
    // TODO: when install extra module, we should check the same version of main(binary/runtime)
    // module has been installed or not

    // Note: same as InstallRef, we should fuzzy the id instead of version
    auto fuzzyRef = package::FuzzyReference::parse(packageRef.id);
    if (!fuzzyRef) {
        return toDBusReply(fuzzyRef);
    }

    auto localRef = this->repo.clearReference(*fuzzyRef,
                                              {
                                                .fallbackToRemote = false // NOLINT
                                              });
    if (localRef) {
        auto layerDir = this->repo.getLayerDir(*localRef, packageInfo.packageInfoV2Module);
        if (layerDir && layerDir->valid()) {
            additionalMessage.localRef = localRef->toString();
        }
    }

    if (!additionalMessage.localRef.empty()) {
        if (packageRef.version == localRef->version) {
            return toDBusReply(utils::error::ErrorCode::Failed,
                               localRef->toString() + " is already installed");
        }

        if (packageRef.version > localRef->version) {
            msgType = api::types::v1::InteractionMessageType::Upgrade;
        } else if (!options.force) {
            auto layerName = fmt::format("{}_{}_{}_{}.layer",
                                         packageRef.id,
                                         packageRef.version.toString(),
                                         architectureRet->toString(),
                                         packageInfo.packageInfoV2Module.c_str());
            auto err = fmt::format("The latest version has been installed. If you want to "
                                   "replace it, try using 'll-cli install {} --force'",
                                   layerName);
            return toDBusReply(utils::error::ErrorCode::Failed, err);
        }
    }

    auto installer =
      [this,
       fdDup = fd, // keep file descriptor don't close by the destructor of QDBusUnixFileDescriptor
       packageRef,
       layerFile = *layerFileRet,
       module = packageInfo.packageInfoV2Module,
       options,
       msgType,
       additionalMessage,
       localRef = localRef ? std::make_optional(*localRef) : std::nullopt](Task &task) {
          PackageTask &taskRef = dynamic_cast<PackageTask &>(task);
          if (msgType == api::types::v1::InteractionMessageType::Upgrade
              && !options.skipInteraction) {
              Q_EMIT RequestInteraction(QDBusObjectPath(taskRef.taskObjectPath().c_str()),
                                        static_cast<int>(msgType),
                                        utils::serialize::toQVariantMap(additionalMessage));
              QEventLoop loop;
              auto conn = connect(
                this,
                &PackageManager::ReplyReceived,
                [&taskRef, &loop](const QVariantMap &reply) {
                    // handle reply
                    auto interactionReply =
                      utils::serialize::fromQVariantMap<api::types::v1::InteractionReply>(reply);
                    if (interactionReply->action != "yes") {
                        taskRef.updateState(linglong::api::types::v1::State::Canceled, "canceled");
                    }

                    loop.exit(0);
                });
              loop.exec();

              disconnect(conn);
          }
          if (taskRef.isTaskDone()) {
              return;
          }

          taskRef.updateState(linglong::api::types::v1::State::Processing, "installing layer");

          taskRef.updateProgress(10);
          package::LayerPackager layerPackager;
          auto layerDir = layerPackager.unpack(*layerFile);
          if (!layerDir) {
              taskRef.reportError(std::move(layerDir).error());
              return;
          }

          auto info = (*layerDir).info();
          if (!info) {
              taskRef.reportError(std::move(info).error());
              return;
          }

          taskRef.updateProgress(30);
          if (info->kind == "app" && (module == "binary" || module == "runtime")) {
              auto res = installAppDepends(taskRef, *info);
              if (!res) {
                  taskRef.reportError(std::move(res).error());
                  return;
              }
          }

          taskRef.updateProgress(60);
          auto result = this->repo.importLayerDir(*layerDir);
          if (!result) {
              taskRef.reportError(std::move(result).error());
              return;
          }

          // develop module only need to import
          if (module != "binary" && module != "runtime") {
              taskRef.updateState(linglong::api::types::v1::State::Succeed,
                                  "install layer successfully");
              return;
          }

          taskRef.updateState(linglong::api::types::v1::State::Succeed,
                              "install layer successfully");

          if (info->kind != "app") {
              return;
          }

          auto newRef = package::Reference::fromPackageInfo(*info);
          if (!newRef) {
              taskRef.reportError(std::move(newRef).error());
              return;
          }

          auto ret = executePostInstallHooks(*newRef);
          if (!ret) {
              taskRef.reportError(std::move(ret).error());
              return;
          }

          if (!localRef) {
              auto res = applyApp(*newRef);
              if (!res) {
                  taskRef.reportError(std::move(res).error());
              }
              return;
          }

          auto modules = this->repo.getModuleList(*localRef);
          if (std::find(modules.cbegin(), modules.cend(), module) == modules.cend()) {
              return;
          }

          ret = switchAppVersion(*localRef, *newRef, true);
          if (!ret) {
              LogE("failed to remove old reference {} after install {}: {}",
                   localRef->toString(),
                   packageRef.toString(),
                   ret.error().message());
          }
      };

    auto taskRet = tasks.addPackageTask(std::move(installer), connection());
    if (!taskRet) {
        return toDBusReply(taskRet);
    }

    auto &taskRef = taskRet->get();
    Q_EMIT TaskAdded(QDBusObjectPath{ taskRef.taskObjectPath().c_str() });
    taskRef.updateState(linglong::api::types::v1::State::Queued, "queued to install from layer");
    return utils::serialize::toQVariantMap(api::types::v1::PackageManager1PackageTaskResult{
      .taskObjectPath = taskRef.taskObjectPath(),
      .code = 0,
      .message = (realFile + " is now installing").toStdString(),
    });
}

QVariantMap PackageManager::installFromUAB(const QDBusUnixFileDescriptor &fd,
                                           const api::types::v1::CommonOptions &options) noexcept
{
    std::unique_ptr<utils::InstallHookManager> installHookManager =
      std::make_unique<utils::InstallHookManager>();

    auto ret = installHookManager->parseInstallHooks();
    if (!ret) {
        return toDBusReply(ret);
    }

    ret = installHookManager->executeInstallHooks(fd.fileDescriptor());
    if (!ret) {
        return toDBusReply(utils::error::ErrorCode::Failed,
                           "uab package signature verification failed.");
    }

    auto action = UabInstallationAction::create(fd.fileDescriptor(), *this, repo, options);
    if (!action) {
        return toDBusReply(utils::error::ErrorCode::Failed,
                           "failed to create uab installation action");
    }

    return runActionOnTaskQueue(action);
}

auto PackageManager::InstallFromFile(const QDBusUnixFileDescriptor &fd,
                                     const QString &fileType,
                                     const QVariantMap &options) noexcept -> QVariantMap
{
    if (!fd.isValid()) {
        return toDBusReply(utils::error::ErrorCode::Failed, "invalid file descriptor");
    }

    auto opts = utils::serialize::fromQVariantMap<api::types::v1::CommonOptions>(options);
    if (!opts) {
        return toDBusReply(opts);
    }

    const static QHash<QString,
                       QVariantMap (PackageManager::*)(
                         const QDBusUnixFileDescriptor &,
                         const api::types::v1::CommonOptions &) noexcept>
      installers = { { "layer", &PackageManager::installFromLayer },
                     { "uab", &PackageManager::installFromUAB } };

    if (!installers.contains(fileType)) {
        auto msg = fmt::format("{} is unsupported fileType", fileType.toStdString());
        return toDBusReply(utils::error::ErrorCode::AppInstallUnsupportedFileFormat, msg);
    }

    return std::invoke(installers[fileType], this, fd, *opts);
}

auto PackageManager::Install(const QVariantMap &parameters) noexcept -> QVariantMap
{
    auto paras =
      utils::serialize::fromQVariantMap<api::types::v1::PackageManager1InstallParameters>(
        parameters);
    if (!paras) {
        return toDBusReply(utils::error::ErrorCode::AppInstallFailed, paras.error().message());
    }

    const auto &package = paras->package;
    auto fuzzyRef =
      package::FuzzyReference::create(package.channel, package.id, package.version, std::nullopt);
    if (!fuzzyRef) {
        return toDBusReply(utils::error::ErrorCode::AppInstallFailed, fuzzyRef.error().message());
    }

    // install binary module by default
    auto modules = package.modules.value_or(std::vector<std::string>{ "binary" });

    std::optional<repo::Repo> usedRepo;
    if (paras->repo) {
        auto repo = this->repo.getRepoByAlias(*paras->repo);
        if (!repo) {
            return toDBusReply(utils::error::ErrorCode::AppInstallFailed, repo.error().message());
        }
        usedRepo = std::move(repo).value();
    }

    LogI("install {} {} from {}",
         fuzzyRef->toString(),
         common::strings::join(modules),
         usedRepo ? usedRepo->name : "(all)");

    auto action = RefInstallationAction::create(*fuzzyRef,
                                                modules,
                                                *this,
                                                repo,
                                                paras->options,
                                                std::move(usedRepo));
    if (!action) {
        return toDBusReply(utils::error::ErrorCode::AppInstallFailed, "");
    }

    return runActionOnTaskQueue(action);
}

auto PackageManager::Uninstall(const QVariantMap &parameters) noexcept -> QVariantMap
{
    auto paras =
      utils::serialize::fromQVariantMap<api::types::v1::PackageManager1UninstallParameters>(
        parameters);
    if (!paras) {
        return toDBusReply(utils::error::ErrorCode::AppUninstallFailed, paras.error().message());
    }

    auto query = linglong::repo::repoCacheQuery{ .id = paras->package.id,
                                                 .channel = paras->package.channel,
                                                 .version = paras->package.version };
    auto candidate = this->repo.listLocalBy(query);
    if (!candidate) {
        return toDBusReply(utils::error::ErrorCode::AppUninstallFailed,
                           candidate.error().message());
    }

    int count = 0;
    std::optional<package::Reference> mainRef{ std::nullopt };
    std::string mainKind;
    for (const auto &item : *candidate) {
        // binary and runtime are both valid main modules
        if (item.info.packageInfoV2Module == "binary"
            || item.info.packageInfoV2Module == "runtime") {
            if (!mainRef) {
                auto ref = package::Reference::fromPackageInfo(item.info);
                if (ref) {
                    mainRef = *ref;
                    mainKind = item.info.kind;
                } else {
                    LogW("invalid package info: {}", ref.error());
                }
            }
            count++;
        }
    }

    if ((mainKind == "base" || mainKind == "runtime") && !paras->options.force) {
        return toDBusReply(utils::error::ErrorCode::AppUninstallBaseOrRuntime,
                           "base or runtime package cannot be uninstalled");
    }

    if (!mainRef) {
        return toDBusReply(utils::error::ErrorCode::AppUninstallNotFoundFromLocal,
                           "the package is not installed");
    }

    if (count > 1) {
        std::vector<std::string> items;
        for (const auto &item : *candidate) {
            if (item.info.packageInfoV2Module == "binary"
                || item.info.packageInfoV2Module == "runtime") {
                auto ref = package::Reference::fromPackageInfo(item.info);
                if (ref) {
                    items.emplace_back(ref->toString());
                } else {
                    items.emplace_back("invalid ref");
                }
            }
        }
        return toDBusReply(utils::error::ErrorCode::AppUninstallMultipleVersions,
                           common::strings::join(items, '\n'));
    }

    auto runningRef = isRefBusy(*mainRef);
    if (!runningRef) {
        return toDBusReply(utils::error::ErrorCode::AppUninstallFailed,
                           fmt::format("failed to get the state of ref {}: {}",
                                       mainRef->toString(),
                                       runningRef.error()));
    }

    if (*runningRef) {
        return toDBusReply(utils::error::ErrorCode::AppUninstallAppIsRunning, "ref is busy");
    }

    auto curModule = paras->package.packageManager1PackageModule.value_or("binary");
    auto refSpec = fmt::format("{}/{}/{}/{}",
                               mainRef->channel,
                               mainRef->id,
                               mainRef->arch.toString(),
                               curModule);

    auto taskRet = tasks.addPackageTask(
      [this, mainRef = *mainRef, curModule](Task &taskRef) {
          if (taskRef.isTaskDone()) {
              return;
          }

          auto res = this->Uninstall(dynamic_cast<PackageTask &>(taskRef), mainRef, curModule);
          if (!res) {
              LogE("uninstall failed: {}", res.error());
              taskRef.reportError(std::move(res.error()));
          }
      },
      connection());
    if (!taskRet) {
        return toDBusReply(taskRet);
    }

    auto &taskRef = taskRet->get();
    Q_EMIT TaskAdded(QDBusObjectPath{ taskRef.taskObjectPath().c_str() });
    taskRef.updateState(linglong::api::types::v1::State::Queued, "queued to uninstall");
    return utils::serialize::toQVariantMap(api::types::v1::PackageManager1PackageTaskResult{
      .taskObjectPath = taskRef.taskObjectPath(),
      .code = 0,
      .message = refSpec + " is now uninstalling",
    });
}

utils::error::Result<void> PackageManager::Uninstall(PackageTask &taskContext,
                                                     const package::Reference &ref,
                                                     const std::string &module) noexcept
{
    LINGLONG_TRACE(fmt::format("uninstall ref {} {}", ref.toString(), module));

    taskContext.updateState(api::types::v1::State::Processing, "start to uninstalling package");

    std::vector<std::string> removedModules{ module };

    if (module == "binary" || module == "runtime") {
        // remove main module means remove all modules
        removedModules = this->repo.getModuleList(ref);

        auto item = repo.getLayerItem(ref);
        if (!item) {
            return LINGLONG_ERR(item);
        }

        if (item->info.kind == "app") {
            auto res = unapplyApp(ref);
            if (!res) {
                return LINGLONG_ERR(res);
            }
        }
    }

    auto res = uninstallRef(ref, removedModules);
    if (!res) {
        return LINGLONG_ERR(res);
    }

    taskContext.updateState(linglong::api::types::v1::State::Succeed,
                            fmt::format("Uninstall {} {} success", ref.toString(), module));

    auto mergeRet = this->repo.mergeModules();
    if (!mergeRet.has_value()) {
        qCritical() << "merge modules failed: " << mergeRet.error().message();
    }

    return LINGLONG_OK;
}

auto PackageManager::Update(const QVariantMap &parameters) noexcept -> QVariantMap
{
    auto paras = utils::serialize::fromQVariantMap<api::types::v1::PackageManager1UpdateParameters>(
      parameters);
    if (!paras) {
        return toDBusReply(utils::error::ErrorCode::AppUpgradeFailed, paras.error().message());
    }

    auto action = PackageUpdateAction::create(paras->packages, paras->depsOnly, *this, repo);
    if (!action) {
        return toDBusReply(utils::error::ErrorCode::AppUpgradeFailed,
                           "failed to create update action");
    }

    return runActionOnTaskQueue(action);
}

utils::error::Result<void> PackageManager::installRef(Task &task,
                                                      const package::ReferenceWithRepo &ref,
                                                      std::vector<std::string> modules) noexcept
{
    LINGLONG_TRACE(fmt::format("install ref {}", ref.reference.toString()));

    if (modules.empty()) {
        return LINGLONG_OK;
    }

    TaskContainer taskContainer(task, modules.size());

    utils::Transaction transaction;
    for (const auto &module : modules) {
        auto &taskPart = taskContainer.next();
        if (repo.isMarkedDeleted(ref.reference, module)) {
            auto res = repo.markDeleted(ref.reference, false, module);
            if (res) {
                transaction.addRollBack([this, &ref, module]() noexcept {
                    auto res = repo.markDeleted(ref.reference, true, module);
                    if (!res) {
                        LogW("failed to roll back unmark deleted {} {}",
                             ref.reference.toString(),
                             module);
                    }
                });
                continue;
            }

            LogW(fmt::format("failed to unmark deleted {} {}, try to pull",
                             ref.reference.toString(),
                             module));
        }

        auto res = repo.pull(taskPart, ref.reference, module, ref.repo);
        if (!res) {
            return LINGLONG_ERR(res);
        }

        res = executePostInstallHooks(ref.reference);
        if (!res) {
            LogW(fmt::format("failed to execute postInstall hooks {}", ref.reference.toString()));
        }
    }

    transaction.commit();
    return LINGLONG_OK;
}

utils::error::Result<bool> PackageManager::tryUninstallRef(const package::Reference &ref) noexcept
{
    LINGLONG_TRACE(fmt::format("try uninstall ref {}", ref.toString()));

    utils::Transaction transaction;
    auto busy = this->isRefBusy(ref);
    if (!busy) {
        return LINGLONG_ERR(busy.error());
    }

    if (*busy) {
        auto modules = repo.getModuleList(ref);
        for (const auto &module : modules) {
            auto res = repo.markDeleted(ref, true, module);
            if (res) {
                transaction.addRollBack([this, &ref, module]() noexcept {
                    auto res = repo.markDeleted(ref, false, module);
                    if (!res) {
                        LogW(fmt::format("failed to roll back mark deleted {} {}",
                                         ref.toString(),
                                         module));
                    }
                });
            }
        }
    } else {
        auto res = uninstallRef(ref);
        if (!res) {
            return LINGLONG_ERR(res.error());
        }
    }
    transaction.commit();
    return !*busy;
}

utils::error::Result<void> PackageManager::uninstallRef(
  const package::Reference &ref, std::optional<std::vector<std::string>> modules) noexcept
{
    LINGLONG_TRACE(fmt::format("uninstall ref {}", ref.toString()));

    if (!modules) {
        modules = this->repo.getModuleList(ref);
    }

    LogD("uninstall ref {} modules: {}",
         ref.toString(),
         common::strings::join(modules.value(), ','));

    for (const auto &module : modules.value()) {
        auto res = this->repo.remove(ref, module);
        if (!res) {
            LogW(fmt::format("failed to remove {} {}: {}", ref.toString(), module, res.error()));
            continue;
        }

        res = executePostUninstallHooks(ref);
        if (!res) {
            LogW(fmt::format("failed to execute postUninstall hooks {}", ref.toString()));
        }
    }

    return LINGLONG_OK;
}

auto PackageManager::Search(const QVariantMap &parameters) noexcept -> QVariantMap
{
    auto paras = utils::serialize::fromQVariantMap<api::types::v1::PackageManager1SearchParameters>(
      parameters);
    if (!paras) {
        return toDBusReply(paras);
    }

    auto task =
      m_search_queue.addPackageTask([this, params = std::move(paras).value()](Task &task) {
          std::map<std::string, std::vector<api::types::v1::PackageInfoV2>> pkgs;
          for (const auto &repoAlias : params.repos) {
              auto repoRet = this->repo.getRepoByAlias(repoAlias);
              if (!repoRet) {
                  LogW("repo {} not found", repoAlias);
                  continue;
              }

              auto pkgInfosRet = this->repo.searchRemote(params.id, *repoRet);
              if (!pkgInfosRet) {
                  LogW("failed to search remote: {}", pkgInfosRet.error());
                  continue;
              }

              if (pkgInfosRet->empty()) {
                  continue;
              }

              pkgs.emplace(repoRet->alias.value_or(repoRet->name), std::move(*pkgInfosRet));
          }

          Q_EMIT this->SearchFinished(
            QString::fromStdString(task.taskID()),
            utils::serialize::toQVariantMap(api::types::v1::PackageManager1SearchResult{
              .packages = std::move(pkgs),
              .code = 0,
              .message = "",
              .type = "",
            }));
      });
    if (!task) {
        return toDBusReply(task);
    }

    auto &taskRef = task->get();
    taskRef.updateState(linglong::api::types::v1::State::Queued,
                        fmt::format("search {}", paras->id));
    auto result = utils::serialize::toQVariantMap(api::types::v1::PackageManager1JobInfo{
      .id = taskRef.taskID(),
      .code = 0,
      .message = "",
      .type = "",
    });
    return result;
}

utils::error::Result<void>
PackageManager::installAppDepends(Task &task, const api::types::v1::PackageInfoV2 &app)
{
    LINGLONG_TRACE(fmt::format("install app depends for {}", app.id));

    auto res = installDependsRef(task, app.base, app.channel);
    if (!res) {
        return LINGLONG_ERR(res);
    }

    if (app.runtime) {
        res = installDependsRef(task, *app.runtime, app.channel);
        if (!res) {
            return LINGLONG_ERR(res);
        }
    }

    return LINGLONG_OK;
}

utils::error::Result<void> PackageManager::installDependsRef(Task &task,
                                                             const std::string &refStr,
                                                             std::optional<std::string> channel,
                                                             std::optional<std::string> version)
{
    LINGLONG_TRACE(fmt::format("install depends ref {}", refStr));

    auto fuzzyRef = package::FuzzyReference::parse(refStr);
    if (!fuzzyRef) {
        return LINGLONG_ERR(fuzzyRef.error());
    }

    // use provided channel/version if not set in fuzzyRef
    if (channel && !fuzzyRef->channel) {
        fuzzyRef->channel = *channel;
    }
    if (version && !fuzzyRef->version) {
        fuzzyRef->version = version;
    }

    auto local = this->repo.clearReference(*fuzzyRef,
                                           {
                                             .forceRemote = false,
                                             .fallbackToRemote = false,
                                             .semanticMatching = true,
                                           });
    // if the ref is already installed, do nothing
    if (local) {
        return LINGLONG_OK;
    }

    auto remote = this->repo.latestRemoteReference(*fuzzyRef);
    if (!remote) {
        return LINGLONG_ERR(remote);
    }

    auto res = installRef(task, *remote, { "binary" });
    if (!res) {
        return LINGLONG_ERR(res);
    }

    return LINGLONG_OK;
}

auto PackageManager::Prune() noexcept -> QVariantMap
{
    auto task = tasks.addTask([this](Task &task) {
        std::vector<api::types::v1::PackageInfoV2> pkgs;
        auto ret = Prune(pkgs);
        if (!ret.has_value()) {
            Q_EMIT PruneFinished(QString::fromStdString(task.taskID()), toDBusReply(ret));
            task.reportError(std::move(ret).error());
            return;
        }

        auto result = api::types::v1::PackageManager1PruneResult{
            .packages = pkgs,
            .code = static_cast<int64_t>(utils::error::ErrorCode::Success),
            .message = "",
        };
        Q_EMIT PruneFinished(QString::fromStdString(task.taskID()),
                             utils::serialize::toQVariantMap(result));
        task.updateState(linglong::api::types::v1::State::Succeed, "prune");
    });
    if (!task) {
        return toDBusReply(task);
    }

    auto &taskRef = task->get();
    taskRef.updateState(linglong::api::types::v1::State::Queued, "prune");
    auto result = utils::serialize::toQVariantMap(api::types::v1::PackageManager1JobInfo{
      .id = taskRef.taskID(),
      .code = 0,
      .message = "",
    });
    return result;
}

utils::error::Result<void>
PackageManager::Prune(std::vector<api::types::v1::PackageInfoV2> &removed) noexcept
{
    LINGLONG_TRACE("prune");
    auto pkgsInfo = this->repo.listLocal();
    if (!pkgsInfo) {
        return LINGLONG_ERR(pkgsInfo);
    }

    std::unordered_map<package::Reference, int> target;

    auto scanExtensionsByInfo = [&target, this](const api::types::v1::PackageInfoV2 &info) {
        if (info.extensions) {
            for (const auto &extension : *info.extensions) {
                std::string name = extension.name;
                auto ext = extension::ExtensionFactory::makeExtension(name);
                if (!ext->shouldEnable(name)) {
                    continue;
                }

                auto fuzzyRef = package::FuzzyReference::create(info.channel,
                                                                name,
                                                                extension.version,
                                                                std::nullopt);
                auto ref = repo.clearReference(
                  *fuzzyRef,
                  { .forceRemote = false, .fallbackToRemote = false, .semanticMatching = true });
                if (ref) {
                    target[*ref] += 1;
                }
            }
        }
    };
    auto scanExtensionsByRef = [scanExtensionsByInfo, this](package::Reference &ref) {
        auto item = this->repo.getLayerItem(ref);
        if (!item) {
            qWarning() << item.error().message();
            return;
        }
        scanExtensionsByInfo(item->info);
    };

    for (const auto &info : *pkgsInfo) {
        if (info.packageInfoV2Module != "binary" && info.packageInfoV2Module != "runtime") {
            continue;
        }

        if (info.kind != "app") {
            auto ref = package::Reference::fromPackageInfo(info);
            if (!ref) {
                qWarning() << ref.error().message();
                continue;
            }
            // Note: if the ref already exists, it's ok, somebody depends it.
            target.try_emplace(std::move(*ref), 0);
            continue;
        }

        if (info.runtime) {
            auto runtimeFuzzyRef = package::FuzzyReference::parse(info.runtime.value());
            if (!runtimeFuzzyRef) {
                qWarning() << runtimeFuzzyRef.error().message();
                continue;
            }

            auto runtimeRef = this->repo.clearReference(*runtimeFuzzyRef,
                                                        {
                                                          .forceRemote = false,
                                                          .fallbackToRemote = false,
                                                          .semanticMatching = true,
                                                        });
            if (!runtimeRef) {
                qWarning() << runtimeRef.error().message();
                continue;
            }
            target[*runtimeRef] += 1;
            scanExtensionsByRef(*runtimeRef);
        }

        auto baseFuzzyRef = package::FuzzyReference::parse(info.base);
        if (!baseFuzzyRef) {
            qWarning() << baseFuzzyRef.error().message();
            continue;
        }

        auto baseRef = this->repo.clearReference(*baseFuzzyRef,
                                                 {
                                                   .forceRemote = false,
                                                   .fallbackToRemote = false,
                                                   .semanticMatching = true,
                                                 });
        if (!baseRef) {
            qWarning() << baseRef.error().message();
            continue;
        }
        target[*baseRef] += 1;
        scanExtensionsByRef(*baseRef);
        scanExtensionsByInfo(info);
    }

    for (const auto &it : target) {
        if (it.second != 0) {
            continue;
        }

        // NOTE: if the binary module is removed, other modules should be removed too.
        for (const auto &module : this->repo.getModuleList(it.first)) {
            auto layer = this->repo.getLayerDir(it.first, module);
            if (!layer) {
                qWarning() << layer.error().message();
                continue;
            }

            auto info = layer->info();

            if (!info) {
                qWarning() << info.error().message();
                continue;
            }

            removed.emplace_back(std::move(*info));

            auto result = this->repo.remove(it.first, module);
            if (!result) {
                return LINGLONG_ERR(result);
            }
        }
    }

    if (!target.empty()) {
        auto mergeRet = this->repo.mergeModules();
        if (!mergeRet.has_value()) {
            qCritical() << "merge modules failed: " << mergeRet.error().message();
        }
    }
    auto pruneRet = this->repo.prune();
    if (!pruneRet) {
        return LINGLONG_ERR(pruneRet);
    }
    return LINGLONG_OK;
}

void PackageManager::ReplyInteraction([[maybe_unused]] QDBusObjectPath object_path,
                                      const QVariantMap &replies)
{
    Q_EMIT this->ReplyReceived(replies);
}

utils::error::Result<void> PackageManager::generateCache(const package::Reference &ref) noexcept
{
    LINGLONG_TRACE("generate cache for " + ref.toString());

    auto layerItem = this->repo.getLayerItem(ref);
    if (!layerItem) {
        return LINGLONG_ERR(layerItem);
    }

    const auto appCache = std::filesystem::path(LINGLONG_ROOT) / "cache" / layerItem->commit;
    const std::string appCacheDest = "/run/linglong/cache";
    const std::string generatorDest = "/run/linglong/generator";

    utils::Transaction transaction;

#ifdef LINGLONG_FONT_CACHE_GENERATOR
    const auto appFontCache = appCache / "fontconfig";
    const std::string fontGenerator = generatorDest + "/font-cache-generator";
#endif
    std::error_code ec;
    std::filesystem::create_directories(appCache, ec);
    if (ec) {
        return LINGLONG_ERR(QString::fromStdString(ec.message()));
    }

    transaction.addRollBack([&appCache]() noexcept {
        std::error_code ec;
        std::filesystem::remove_all(appCache, ec);
        if (ec) {
            qCritical() << QString::fromStdString(ec.message());
        }
    });

    runtime::RunContext runContext(this->repo);
    auto res = runContext.resolve(ref);
    if (!res) {
        return LINGLONG_ERR(res);
    }

    auto uid = getuid();
    auto gid = getgid();

    std::filesystem::path ldConfPath{ appCache / "ld.so.conf" };

    linglong::generator::ContainerCfgBuilder cfgBuilder;
    res = runContext.fillContextCfg(cfgBuilder);
    if (!res) {
        return LINGLONG_ERR(res);
    }

    cfgBuilder.setAppId(ref.id)
      .setAppCache(appCache, false)
      .addUIdMapping(uid, uid, 1)
      .addGIdMapping(gid, gid, 1)
      .bindDefault()
      .bindCgroup()
      .bindXDGRuntime()
      .bindUserGroup()
      .forwardDefaultEnv()
      .addExtraMounts(std::vector<ocppi::runtime::config::types::Mount>{
        ocppi::runtime::config::types::Mount{ .destination = generatorDest,
                                              .options = { { "rbind", "ro" } },
                                              .source = LINGLONG_LIBEXEC_DIR,
                                              .type = "bind" },
        ocppi::runtime::config::types::Mount{
          .destination = "/etc/ld.so.conf.d/zz_deepin-linglong-app.conf",
          .options = { { "rbind", "ro" } },
          .source = ldConfPath,
          .type = "bind",
        } })
      .enableSelfAdjustingMount();
#ifdef LINGLONG_FONT_CACHE_GENERATOR
    cfgBuilder.enableFontCache();
#endif

    // generate ld config
    {
        std::ofstream ofs(ldConfPath, std::ios::binary | std::ios::out | std::ios::trunc);
        Q_ASSERT(ofs.is_open());
        if (!ofs.is_open()) {
            return LINGLONG_ERR("create ld config in bundle directory");
        }
        ofs << cfgBuilder.ldConf(ref.arch.getTriplet());
    }

    if (!cfgBuilder.build()) {
        auto err = cfgBuilder.getError();
        return LINGLONG_ERR("build cfg error: " + QString::fromStdString(err.reason));
    }

    auto container = this->containerBuilder.create(cfgBuilder);
    if (!container) {
        return LINGLONG_ERR(container);
    }

    ocppi::runtime::config::types::Process process{};
    process.cwd = "/";
    process.noNewPrivileges = true;
    process.terminal = true;

    auto ldGenerateCmd =
      std::vector<std::string>{ "/sbin/ldconfig", "-X", "-C", appCacheDest + "/ld.so.cache" };
#ifdef LINGLONG_FONT_CACHE_GENERATOR
    // Usage: font-cache-generator [cacheRoot] [id]
    const std::string fontGenerateCmd = common::strings::quoteBashArg(fontGenerator) + " "
      + common::strings::quoteBashArg(appCacheDest) + " "
      + common::strings::quoteBashArg(ref.id.toStdString());
    auto ldGenerateCmdstr;
    for (const auto &c : ldGenerateCmd) {
        ldGenerateCmdstr.append(common::strings::quoteBashArg(c));
        ldGenerateCmdstr.append(" ");
    }
    process.args =
      std::vector<std::string>{ "bash", "-c", ldGenerateCmdstr + ";" + fontGenerateCmd };
#endif

    process.args = std::move(ldGenerateCmd);
    auto XDGRuntimeDir = common::dir::getAppRuntimeDir(ref.id);
    auto containerStateRoot = XDGRuntimeDir / "ll-box";

    ocppi::runtime::RunOption opt;
    opt.GlobalOption::root = containerStateRoot;
    auto result = (*container)->run(process, opt);
    if (!result) {
        return LINGLONG_ERR(result);
    }

    transaction.commit();
    return LINGLONG_OK;
}

// it's safe to skip cache generation here, if the cache directory already exists.
// when application begins to run, it will regenerate the cache if necessary
utils::error::Result<void> PackageManager::tryGenerateCache(const package::Reference &ref) noexcept
{
    LINGLONG_TRACE("try to generate cache for " + ref.toString());

    auto layerItem = this->repo.getLayerItem(ref);
    if (!layerItem) {
        return LINGLONG_ERR(layerItem);
    }
    auto appCache = std::filesystem::path(LINGLONG_ROOT) / "cache" / layerItem->commit;
    std::error_code ec;
    if (std::filesystem::exists(appCache, ec)) {
        return LINGLONG_OK;
    }
    if (ec) {
        return LINGLONG_ERR(QString::fromStdString(ec.message()));
    }

    auto ret = generateCache(ref);
    if (!ret) {
        qWarning() << "failed to generate cache" << ret.error();
    }

    return LINGLONG_OK;
}

utils::error::Result<void> PackageManager::removeCache(const package::Reference &ref) noexcept
{
    LINGLONG_TRACE("remove the cache of " + ref.toString());

    auto layerItem = this->repo.getLayerItem(ref);
    if (!layerItem) {
        return LINGLONG_ERR(layerItem);
    }

    const auto appCache = std::filesystem::path(LINGLONG_ROOT) / "cache" / layerItem->commit;
    std::error_code ec;
    std::filesystem::remove_all(appCache, ec);
    if (ec) {
        return LINGLONG_ERR("failed to remove cache directory", ec);
    }

    return LINGLONG_OK;
}

auto PackageManager::GenerateCache(const QString &reference) noexcept -> QVariantMap
{
    auto refRet = package::Reference::parse(reference.toStdString());
    if (!refRet) {
        return toDBusReply(refRet);
    }
    auto taskRet =
      m_generator_queue.addPackageTask([this, ref = std::move(refRet).value()](Task &task) {
          LogI("Generate cache for {}", ref.toString());

          auto ret = this->generateCache(ref);
          if (!ret) {
              LogE("failed to generate cache for {}: {}", ref.toString(), ret.error().message());
              Q_EMIT this->GenerateCacheFinished(QString::fromStdString(task.taskID()), false);
              return;
          }

          qInfo() << "Generate cache finished";
          Q_EMIT this->GenerateCacheFinished(QString::fromStdString(task.taskID()), true);
      });
    if (!taskRet) {
        return toDBusReply(taskRet);
    }

    auto result = utils::serialize::toQVariantMap(api::types::v1::PackageManager1JobInfo{
      .id = taskRet->get().taskID(),
      .code = 0,
      .message = "",
    });
    return result;
}

utils::error::Result<void>
PackageManager::executePostInstallHooks(const package::Reference &ref) noexcept
{
    LINGLONG_TRACE("execute post install hooks for: " + ref.toString());

    std::unique_ptr<utils::InstallHookManager> installHookManager =
      std::make_unique<utils::InstallHookManager>();
    auto ret = installHookManager->parseInstallHooks();
    if (!ret) {
        return LINGLONG_ERR(ret);
    }

    auto layerDir = this->repo.getLayerDir(ref);
    if (!layerDir) {
        return LINGLONG_ERR(layerDir);
    }

    auto appPath = layerDir->absolutePath();

    ret = installHookManager->executePostInstallHooks(ref.id, appPath.toStdString());
    if (!ret) {
        return LINGLONG_ERR(ret);
    }

    return LINGLONG_OK;
}

utils::error::Result<void>
PackageManager::executePostUninstallHooks(const package::Reference &ref) noexcept
{
    LINGLONG_TRACE("execute post uninstall hooks for: " + ref.toString());

    std::unique_ptr<utils::InstallHookManager> installHookManager =
      std::make_unique<utils::InstallHookManager>();
    auto ret = installHookManager->parseInstallHooks();
    if (!ret) {
        return LINGLONG_ERR(ret);
    }

    ret = installHookManager->executePostUninstallHooks(ref.id);
    if (!ret) {
        return LINGLONG_ERR(ret);
    }

    return LINGLONG_OK;
}

bool PackageManager::waitConfirm(
  PackageTask &taskRef,
  api::types::v1::InteractionMessageType msgType,
  const api::types::v1::PackageManager1RequestInteractionAdditionalMessage
    &additionalMessage) noexcept
{
    Q_EMIT RequestInteraction(QDBusObjectPath(taskRef.taskObjectPath().c_str()),
                              static_cast<int>(msgType),
                              utils::serialize::toQVariantMap(additionalMessage));
    QEventLoop loop;
    auto conn =
      connect(this, &PackageManager::ReplyReceived, [&taskRef, &loop](const QVariantMap &reply) {
          auto interactionReply =
            utils::serialize::fromQVariantMap<api::types::v1::InteractionReply>(reply);
          if (interactionReply->action != "yes") {
              taskRef.updateState(linglong::api::types::v1::State::Canceled, "canceled");
          }
          loop.exit(0);
      });
    loop.exec();

    disconnect(conn);

    return !taskRef.isTaskDone();
}

QVariantMap PackageManager::runActionOnTaskQueue(std::shared_ptr<Action> action)
{
    // prepare is run within the DBus calling context.
    // doAction is run within the PM's packages task queue context.
    // state consistency cannot be assumed between prepare and doAction.
    // For now, DBus calling context runs on the application's main event loop,
    // and PM's packages task runs on a temporary work thread.
    auto prepared = action->prepare();
    if (!prepared) {
        return toDBusReply(prepared);
    }

    auto taskRet = tasks.addPackageTask(
      [action](Task &task) {
          auto res = action->doAction(dynamic_cast<PackageTask &>(task));
          if (!res) {
              LogE("action {} failed: {}", action->getTaskName(), res.error());
              task.reportError(std::move(res).error());
          } else {
              LogI("action {} succeed: {}", action->getTaskName(), task.Task::message());
          }
      },
      connection());
    if (!taskRet) {
        return toDBusReply(taskRet);
    }

    auto &taskRef = taskRet->get();
    Q_EMIT TaskAdded(QDBusObjectPath{ taskRef.taskObjectPath().c_str() });
    taskRef.updateState(linglong::api::types::v1::State::Queued, action->getTaskName());
    return utils::serialize::toQVariantMap(api::types::v1::PackageManager1PackageTaskResult{
      .taskObjectPath = taskRef.taskObjectPath(),
      .code = 0,
      .message = action->getTaskName() + " is queued",
    });
}

} // namespace linglong::service
