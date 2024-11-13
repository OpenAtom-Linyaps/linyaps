/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "package_manager.h"

#include "linglong/adaptors/migrate/migrate1.h"
#include "linglong/api/types/v1/Generators.hpp"
#include "linglong/api/types/v1/PackageManager1JobInfo.hpp"
#include "linglong/package/layer_file.h"
#include "linglong/package/layer_packager.h"
#include "linglong/package/uab_file.h"
#include "linglong/package_manager/migrate.h"
#include "linglong/package_manager/package_task.h"
#include "linglong/utils/command/env.h"
#include "linglong/utils/dbus/register.h"
#include "linglong/utils/finally/finally.h"
#include "linglong/utils/packageinfo_handler.h"
#include "linglong/utils/serialize/json.h"
#include "linglong/utils/transaction.h"

#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusUnixFileDescriptor>
#include <QDebug>
#include <QEventLoop>
#include <QJsonArray>
#include <QMetaObject>
#include <QUuid>

#include <algorithm>
#include <unordered_set>
#include <utility>

#include <fcntl.h>

namespace linglong::service {

namespace {

constexpr auto repoLockPath = "/run/linglong/lock";

template<typename T>
QVariantMap toDBusReply(const utils::error::Result<T> &x, std::string type = "display") noexcept
{
    Q_ASSERT(!x.has_value());

    return utils::serialize::toQVariantMap(
      api::types::v1::CommonResult{ .code = x.error().code(),                     // NOLINT
                                    .message = x.error().message().toStdString(), // NOLINT
                                    .type = std::move(type) });
}

QVariantMap toDBusReply(int code, const QString &message, std::string type = "display") noexcept
{
    return utils::serialize::toQVariantMap(
      api::types::v1::CommonResult{ .code = code,                     // NOLINT
                                    .message = message.toStdString(), // NOLINT
                                    .type = std::move(type) });
}

bool isTaskDone(linglong::api::types::v1::SubState subState) noexcept
{
    return subState == linglong::api::types::v1::SubState::AllDone
      || subState == linglong::api::types::v1::SubState::PackageManagerDone;
}

utils::error::Result<package::FuzzyReference>
fuzzyReferenceFromPackage(const api::types::v1::PackageManager1Package &pkg) noexcept
{
    std::optional<QString> channel;
    if (pkg.channel) {
        channel = QString::fromStdString(*pkg.channel);
    }

    std::optional<package::Version> version;
    if (pkg.version) {
        auto tmpVersion = package::Version::parse(QString::fromStdString(*pkg.version));
        if (!tmpVersion) {
            return tl::unexpected(std::move(tmpVersion.error()));
        }

        version = *tmpVersion;
    }

    auto fuzzyRef = package::FuzzyReference::create(channel,
                                                    QString::fromStdString(pkg.id),
                                                    version,
                                                    std::nullopt);
    return fuzzyRef;
}
} // namespace

PackageManager::PackageManager(linglong::repo::OSTreeRepo &repo, QObject *parent)
    : QObject(parent)
    , repo(repo)
{
    // exec install on task list changed signal
    connect(
      this,
      &PackageManager::TaskListChanged,
      this,
      [this](const QString &taskObjectPath) {
          // notify task waiting
          if (!this->runningTaskObjectPath.isEmpty()) {
              for (auto *task : taskList) {
                  // skip tasks without job
                  if (!task->getJob().has_value()
                      || task->taskObjectPath() == runningTaskObjectPath) {
                      continue;
                  }
                  auto msg = QString("Waiting for the other tasks");
                  task->updateState(linglong::api::types::v1::State::Queued, msg);
              }
              return;
          }
          // start next task
          this->runningTaskObjectPath = taskObjectPath;
          if (this->taskList.empty()) {
              return;
          };
          for (auto it = taskList.begin(); it != taskList.end(); ++it) {
              auto *task = *it;
              if (!task->getJob().has_value()
                  || task->state() != linglong::api::types::v1::State::Queued) {
                  continue;
              }
              // execute the task
              auto func = *task->getJob();
              func();
              Q_EMIT TaskRemoved(QDBusObjectPath{ task->taskObjectPath() },
                                 static_cast<int>(task->state()),
                                 static_cast<int>(task->subState()),
                                 task->message(),
                                 task->getPercentage());
              this->runningTaskObjectPath = "";
              this->taskList.erase(it);
              task->deleteLater();
              Q_EMIT this->TaskListChanged("");
              return;
          }
      },
      Qt::QueuedConnection);

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
    timer->callOnTimeout([this, timer] {
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
    LINGLONG_TRACE(QString{ "check if ref[%1] is used by some apps" }.arg(ref.toString()));

    auto ret = lockRepo();
    if (!ret) {
        return LINGLONG_ERR(
          QStringLiteral("failed to lock repo, underlying data will not be removed:")
          % ret.error().message());
    }

    auto unlock = utils::finally::finally([this] {
        auto ret = unlockRepo();
        if (!ret) {
            qCritical() << "failed to unlock repo:" << ret.error().message();
        }
    });

    auto running = getAllRunningContainers();
    if (!running) {
        return LINGLONG_ERR(QStringLiteral("failed to get running containers:")
                            % running.error().message());
    }
    auto &runningRef = *running;

    return std::find_if(runningRef.cbegin(),
                        runningRef.cend(),
                        [&ref](const api::types::v1::ContainerProcessStateInfo &info) {
                            return info.app == ref.toString().toStdString();
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
        return LINGLONG_ERR(QStringLiteral("failed to list /run/linglong: ")
                            % ec.message().c_str());
    }

    std::vector<api::types::v1::ContainerProcessStateInfo> result;
    for (const auto &entry : user_iterator) {
        if (!entry.is_directory()) {
            continue;
        }

        auto process_iterator = std::filesystem::directory_iterator{ entry.path(), ec };
        if (ec) {
            return LINGLONG_ERR(QStringLiteral("failed to list ") % entry.path().c_str() % ": "
                                % ec.message().c_str());
        }

        for (const auto &process_entry : process_iterator) {
            if (!process_entry.is_regular_file()) {
                continue;
            }

            auto pid = process_entry.path().filename().string();
            if (auto procDir = "/proc/" + pid; !std::filesystem::exists(procDir, ec)) {
                if (ec) {
                    return LINGLONG_ERR(QStringLiteral("failed to get state of ") % procDir.c_str()
                                        % ": " % ec.message().c_str());
                }

                qInfo() << "ignore" << process_entry.path().c_str()
                        << ",because corrsponding process is not found.";
                continue;
            }

            auto content =
              utils::serialize::LoadJSONFile<api::types::v1::ContainerProcessStateInfo>(
                QString::fromStdString(process_entry.path().string()));
            if (!content) {
                return LINGLONG_ERR(QStringLiteral("failed to load info from ")
                                    % process_entry.path().c_str() % ": "
                                    % content.error().message());
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
        return LINGLONG_ERR(QStringLiteral("failed to create lock file ") % repoLockPath % ": "
                            % ::strerror(errno));
    }

    struct flock locker
    {
        .l_type = F_WRLCK, .l_whence = SEEK_SET, .l_start = 0, .l_len = 0
    };

    if (::fcntl(lockFd, F_SETLK, &locker) == -1) {
        return LINGLONG_ERR(QStringLiteral("failed to lock ") % repoLockPath % ": "
                            % ::strerror(errno));
    }

    return LINGLONG_OK;
}

[[nodiscard]] utils::error::Result<void> PackageManager::unlockRepo() noexcept
{
    LINGLONG_TRACE("unlock whole repo")

    if (lockFd == -1) {
        return LINGLONG_OK;
    }

    struct flock unlocker
    {
        .l_type = F_UNLCK, .l_whence = SEEK_SET, .l_start = 0, .l_len = 0
    };

    if (::fcntl(lockFd, F_SETLK, &unlocker)) {
        return LINGLONG_ERR(QStringLiteral("failed to unlock ") % repoLockPath % ": "
                            % ::strerror(errno));
    }

    ::close(lockFd);
    lockFd = -1;

    return LINGLONG_OK;
}

utils::error::Result<void> PackageManager::removeAfterInstall(
  const package::Reference &oldRef, const std::vector<std::string> &modules) noexcept
{
    LINGLONG_TRACE("remove old reference after install")

    auto needDelayRet = isRefBusy(oldRef);
    if (!needDelayRet) {
        return LINGLONG_ERR(needDelayRet);
    }

    utils::Transaction transaction;
    this->repo.unexportReference(oldRef);
    transaction.addRollBack([&oldRef, this]() noexcept {
        this->repo.exportReference(oldRef);
    });

    for (const auto &module : modules) {
        if (*needDelayRet) {
            auto ret = this->repo.markDeleted(oldRef, true, module);
            if (!ret) {
                return LINGLONG_ERR("Failed to mark old reference " % oldRef.toString()
                                      % " as deleted",
                                    ret);
            }

            transaction.addRollBack([this, &oldRef, module]() noexcept {
                auto ret = this->repo.markDeleted(oldRef, false, module);
                if (!ret) {
                    qWarning() << "Failed to rollback marking old reference " << oldRef.toString()
                               << ":" << ret.error();
                }
            });
        } else {
            auto ret = this->repo.remove(oldRef);
            if (!ret) {
                return LINGLONG_ERR("Failed to remove old reference " % oldRef.toString(), ret);
            }

            transaction.addRollBack([this, &oldRef, module]() noexcept {
                auto tmp = PackageTask::createTemporaryTask();
                this->repo.pull(tmp, oldRef, module);
                if (tmp.state() != linglong::api::types::v1::State::Succeed) {
                    qWarning() << "failed to rollback remove old reference" << oldRef.toString()
                               << ":" << tmp.message();
                }
            });
        }
    }

    transaction.commit();
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
        qCritical() << "failed to list deleted layers" << uninstalled.error().message();
        return;
    }

    std::unordered_set<std::string> uninstalledLayers;
    for (const auto &item : *uninstalled) {
        auto ref = package::Reference::fromPackageInfo(item.info);
        if (!ref) {
            qCritical() << "underlying storage was broken, exit.";
            Q_ASSERT(false);
            return;
        }

        uninstalledLayers.emplace(ref->toString().toStdString());
    }

    if (uninstalledLayers.empty()) {
        return;
    }

    // retrieve running info
    auto running = getAllRunningContainers();
    if (!running) {
        qCritical() << "failed to get all running containers:" << running.error().message();
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

    // begin to uninstall
    for (const auto &refStr : uninstalledLayers) {
        auto ref = package::Reference::parse(QString::fromStdString(refStr));
        if (!ref) {
            qCritical() << "internal error:" << ref.error().message();
            Q_ASSERT(false);
            return;
        }

        this->repo.unexportReference(*ref);
        auto ret = this->repo.remove(*ref);
        if (!ret) {
            qCritical() << "failed to remove reference:" << ref->toString() << ":"
                        << ret.error().message();
            continue;
        }
    }

    auto mergeRet = this->repo.mergeModules();
    if (!mergeRet.has_value()) {
        qCritical() << "merge modules failed: " << mergeRet.error().message();
    }
}

auto PackageManager::getConfiguration() const noexcept -> QVariantMap
{
    return utils::serialize::toQVariantMap(this->repo.getConfig());
}

void PackageManager::setConfiguration(const QVariantMap &parameters) noexcept
{
    auto cfg = utils::serialize::fromQVariantMap<api::types::v1::RepoConfig>(parameters);
    if (!cfg) {
        sendErrorReply(QDBusError::InvalidArgs, cfg.error().message());
        return;
    }

    const auto &cfgRef = *cfg;
    const auto &curCfg = repo.getConfig();
    if (cfgRef.version == curCfg.version && cfgRef.defaultRepo == curCfg.defaultRepo
        && cfgRef.repos == curCfg.repos) {
        return;
    }

    if (const auto &defaultRepo = cfg->defaultRepo;
        cfg->repos.find(defaultRepo) == cfg->repos.end()) {
        sendErrorReply(QDBusError::Failed,
                       "default repository is missing after updating configuration.");
        return;
    }

    auto result = this->repo.setConfig(*cfg);
    if (!result) {
        sendErrorReply(QDBusError::Failed, result.error().message());
    }
}

QVariantMap PackageManager::installFromLayer(const QDBusUnixFileDescriptor &fd,
                                             const api::types::v1::CommonOptions &options) noexcept
{
    auto layerFileRet =
      package::LayerFile::New(QString("/proc/%1/fd/%2").arg(getpid()).arg(fd.fileDescriptor()));
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

    auto currentArch = package::Architecture::currentCPUArchitecture();
    if (!currentArch) {
        return toDBusReply(currentArch);
    }

    if (*architectureRet != *currentArch) {
        return toDBusReply(-1,
                           "app arch:" + architectureRet->toString()
                             + " not match host architecture");
    }

    auto versionRet = package::Version::parse(QString::fromStdString(packageInfo.version));
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

    additionalMessage.remoteRef = packageRef.toString().toStdString();

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
            additionalMessage.localRef = localRef->toString().toStdString();
        }
    }

    if (!additionalMessage.localRef.empty()) {
        if (packageRef.version == localRef->version) {
            return toDBusReply(-1, localRef->toString() + " is already installed");
        }

        if (packageRef.version > localRef->version) {
            msgType = api::types::v1::InteractionMessageType::Upgrade;
        } else if (!options.force) {
            return toDBusReply(-1,
                               "The latest version has been installed. If you need to "
                               "overwrite it, try using '--force'");
        }
    }

    auto refSpec =
      QString{ "%1/%2/%3" }.arg("local",
                                packageRef.toString(),
                                QString::fromStdString(packageInfo.packageInfoV2Module));
    auto task = std::find_if(this->taskList.cbegin(),
                             this->taskList.cend(),
                             [&refSpec](const PackageTask *task) {
                                 return task->isRefExist(refSpec);
                             });
    if (task != this->taskList.cend()) {
        return toDBusReply(-1, "the target " % refSpec % " is being operated");
    }
    auto *taskPtr = new PackageTask{ connection(), QStringList{ refSpec } };
    auto &taskRef = *(this->taskList.emplace_back(taskPtr));

    auto installer =
      [this,
       fdDup = fd, // keep file descriptor don't close by the destructor of QDBusUnixFileDescriptor
       &taskRef,
       packageRef,
       layerFile = *layerFileRet,
       module = packageInfo.packageInfoV2Module,
       options,
       msgType,
       additionalMessage,
       localRef = localRef ? std::make_optional(*localRef) : std::nullopt]() {
          if (msgType == api::types::v1::InteractionMessageType::Upgrade
              && !options.skipInteraction) {
              Q_EMIT RequestInteraction(QDBusObjectPath(taskRef.taskObjectPath()),
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
          if (isTaskDone(taskRef.subState())) {
              return;
          }

          taskRef.updateState(linglong::api::types::v1::State::Processing, "installing layer");
          taskRef.updateSubState(linglong::api::types::v1::SubState::PreAction,
                                 "preparing environment");

          package::LayerPackager layerPackager;
          auto layerDir = layerPackager.unpack(*layerFile);
          if (!layerDir) {
              taskRef.reportError(std::move(layerDir).error());
              return;
          }

          auto unmountLayer = utils::finally::finally([mountPoint = layerDir->absolutePath()] {
              if (QFileInfo::exists(mountPoint)) {
                  auto ret = utils::command::Exec("umount", { mountPoint });
                  if (!ret) {
                      qCritical() << "failed to umount " << mountPoint
                                  << ", please umount it manually";
                  }
              }
          });

          auto info = (*layerDir).info();
          if (!info) {
              taskRef.reportError(std::move(info).error());
              return;
          }

          pullDependency(taskRef, *info, module);
          if (isTaskDone(taskRef.subState())) {
              return;
          }

          auto result = this->repo.importLayerDir(*layerDir);
          if (!result) {
              taskRef.reportError(std::move(result).error());
              return;
          }

          this->repo.exportReference(packageRef);

          auto mergeRet = this->repo.mergeModules();
          if (!mergeRet.has_value()) {
              qCritical() << "merge modules failed: " << mergeRet.error().message();
          }
          taskRef.updateState(linglong::api::types::v1::State::Succeed,
                              "install layer successfully");

          if (localRef) {
              auto modules = this->repo.getModuleList(*localRef);
              if (std::find(modules.cbegin(), modules.cend(), module) == modules.cend()) {
                  return;
              }

              auto ret = removeAfterInstall(*localRef, std::vector{ module });
              if (!ret) {
                  qCritical() << "failed to remove old reference" << localRef->toString()
                              << "after install" << packageRef.toString() << ":"
                              << ret.error().message();
              }
          }
      };
    taskRef.setJob(std::move(installer));

    Q_EMIT TaskAdded(QDBusObjectPath{ taskRef.taskObjectPath() });
    Q_EMIT TaskListChanged(taskRef.taskObjectPath());
    return utils::serialize::toQVariantMap(api::types::v1::PackageManager1PackageTaskResult{
      .taskObjectPath = taskRef.taskObjectPath().toStdString(),
      .code = 0,
      .message = (realFile + " is now installing").toStdString(),
    });
}

QVariantMap PackageManager::installFromUAB(const QDBusUnixFileDescriptor &fd,
                                           const api::types::v1::CommonOptions &options) noexcept
{
    auto uabRet = package::UABFile::loadFromFile(
      QString("/proc/%1/fd/%2").arg(getpid()).arg(fd.fileDescriptor()));
    if (!uabRet) {
        return toDBusReply(uabRet);
    }

    const auto &uab = *uabRet;
    auto verifyRet = uab->verify();
    if (!verifyRet) {
        return toDBusReply(verifyRet);
    }
    if (!*verifyRet) {
        return toDBusReply(-1, "couldn't pass uab verification");
    }

    auto realFile = uab->symLinkTarget();

    auto metaInfoRet = uab->getMetaInfo();
    if (!metaInfoRet) {
        return toDBusReply(metaInfoRet);
    }

    const auto &metaInfo = *metaInfoRet;
    auto layerInfos = metaInfo.get().layers;
    auto appLayerIt = std::find_if(layerInfos.cbegin(),
                                   layerInfos.cend(),
                                   [](const api::types::v1::UabLayer &layer) {
                                       return layer.info.kind == "app";
                                   });
    if (appLayerIt == layerInfos.cend()) {
        return toDBusReply(-1, "couldn't find application layer in this uab");
    }
    auto appLayer = *appLayerIt;

    auto architectureRet = package::Architecture::parse(appLayer.info.arch[0]);
    if (!architectureRet) {
        return toDBusReply(architectureRet);
    }

    auto currentArch = package::Architecture::currentCPUArchitecture();
    if (!currentArch) {
        return toDBusReply(currentArch);
    }

    if (*architectureRet != *currentArch) {
        return toDBusReply(-1,
                           "app arch:" + architectureRet->toString()
                             + " not match host architecture");
    }

    auto versionRet = package::Version::parse(QString::fromStdString(appLayer.info.version));
    if (!versionRet) {
        return toDBusReply(versionRet);
    }

    auto appRefRet = package::Reference::fromPackageInfo(appLayer.info);
    if (!appRefRet) {
        return toDBusReply(appRefRet);
    }

    const auto &appRef = *appRefRet;

    api::types::v1::PackageManager1RequestInteractionAdditionalMessage additionalMessage;
    api::types::v1::InteractionMessageType msgType =
      api::types::v1::InteractionMessageType::Install;

    additionalMessage.remoteRef = appRef.toString().toStdString();

    // Note: same as InstallRef, we should fuzzy the id instead of version
    auto fuzzyRef = package::FuzzyReference::parse(appRef.id);
    if (!fuzzyRef) {
        return toDBusReply(fuzzyRef);
    }

    auto localAppRef = this->repo.clearReference(*fuzzyRef,
                                                 {
                                                   .fallbackToRemote = false // NOLINT
                                                 });
    if (localAppRef) {
        auto layerDir = this->repo.getLayerDir(*localAppRef, appLayer.info.packageInfoV2Module);
        if (layerDir && layerDir->valid()) {
            additionalMessage.localRef = localAppRef->toString().toStdString();
        }
    }

    if (!additionalMessage.localRef.empty()) {
        if (appRef.version == localAppRef->version) {
            return toDBusReply(-1, localAppRef->toString() + " is already installed");
        }

        if (appRef.version > localAppRef->version) {
            msgType = api::types::v1::InteractionMessageType::Upgrade;
        } else if (!options.force) {
            return toDBusReply(-1,
                               "The latest version has been installed. If you need to "
                               "overwrite it, try using '--force'");
        }
    }

    auto refSpec =
      QString{ "%1/%2/%3" }.arg("local",
                                appRef.toString(),
                                QString::fromStdString(appLayer.info.packageInfoV2Module));
    auto task = std::find_if(this->taskList.cbegin(),
                             this->taskList.cend(),
                             [&refSpec](const PackageTask *task) {
                                 return task->isRefExist(refSpec);
                             });
    if (task != this->taskList.cend()) {
        return toDBusReply(-1, "the target " % refSpec % " is being operated");
    }
    auto *taskPtr = new PackageTask{ connection(), QStringList{ refSpec } };
    auto &taskRef = *(this->taskList.emplace_back(taskPtr));

    layerInfos.erase(appLayerIt);
    layerInfos.insert(layerInfos.begin(),
                      std::move(appLayer)); // app layer should place to the first of vector
    std::optional<package::Reference> oldAppRef;
    if (localAppRef) {
        oldAppRef = std::move(localAppRef).value();
    }

    auto installer = [this,
                      &taskRef,
                      fdDup = fd, // keep file descriptor don't close by the destructor of
                                  // QDBusUnixFileDescriptor
                      uab = std::move(uabRet).value(),
                      layerInfos = std::move(layerInfos),
                      metaInfo = std::move(metaInfoRet).value(),
                      appRef = std::move(appRefRet).value(),
                      options,
                      msgType,
                      additionalMessage,
                      oldAppRef = std::move(oldAppRef)] {
        if (msgType == api::types::v1::InteractionMessageType::Upgrade
            && !options.skipInteraction) {
            Q_EMIT RequestInteraction(QDBusObjectPath(taskRef.taskObjectPath()),
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
        if (isTaskDone(taskRef.subState())) {
            return;
        }

        taskRef.updateState(linglong::api::types::v1::State::Processing, "installing uab");
        taskRef.updateSubState(linglong::api::types::v1::SubState::PreAction,
                               "prepare environment");

        auto mountPoint = uab->mountUab();
        if (!mountPoint) {
            taskRef.reportError(std::move(mountPoint).error());
            return;
        }

        if (isTaskDone(taskRef.subState())) {
            return;
        }

        const auto &uabLayersDirInfo = QFileInfo{ mountPoint->absoluteFilePath("layers") };
        if (!uabLayersDirInfo.exists() || !uabLayersDirInfo.isDir()) {
            taskRef.updateState(linglong::api::types::v1::State::Failed,
                                "the contents of this uab file are invalid");
            return;
        }

        utils::Transaction transaction;
        const auto &uabLayersDir = QDir{ uabLayersDirInfo.absoluteFilePath() };
        package::LayerDir appLayerDir;
        for (const auto &layer : layerInfos) {
            if (isTaskDone(taskRef.subState())) {
                return;
            }

            QDir layerDirPath = uabLayersDir.absoluteFilePath(
              QString::fromStdString(layer.info.id) % QDir::separator()
              % QString::fromStdString(layer.info.packageInfoV2Module));

            if (!layerDirPath.exists()) {
                taskRef.updateState(linglong::api::types::v1::State::Failed,
                                    "layer directory " % layerDirPath.absolutePath()
                                      % " doesn't exist");
                return;
            }

            const auto &layerDir = package::LayerDir{ layerDirPath.absolutePath() };
            std::optional<std::string> subRef{ std::nullopt };
            if (layer.minified) {
                subRef = metaInfo.get().uuid;
            }

            auto infoRet = layerDir.info();
            if (!infoRet) {
                taskRef.reportError(std::move(infoRet).error());
                return;
            }
            auto &info = *infoRet;

            auto refRet = package::Reference::fromPackageInfo(info);
            if (!refRet) {
                taskRef.reportError(std::move(refRet).error());
                return;
            }
            auto &ref = *refRet;

            bool isAppLayer = layer.info.kind == "app";
            if (isAppLayer) { // it's meaningless for app layer that declare minified is true
                subRef = std::nullopt;
            } else {
                auto fuzzyString = refRet->id + "/" + refRet->version.toString();
                auto fuzzyRef = package::FuzzyReference::parse(fuzzyString);
                auto localRef = this->repo.clearReference(*fuzzyRef,
                                                          {
                                                            .fallbackToRemote = false // NOLINT
                                                          });
                if (localRef) {
                    auto layerDir = this->repo.getLayerDir(*localRef, info.packageInfoV2Module);
                    if (layerDir && layerDir->valid() && refRet->version == localRef->version) {
                        // if the completed reference of local installed has the same version,
                        // skip it
                        continue;
                    }
                }
            }

            auto ret = this->repo.importLayerDir(layerDir, subRef);
            if (!ret) {
                taskRef.reportError(std::move(ret).error());
                return;
            }

            transaction.addRollBack(
              [this, layerInfo = std::move(info), layerRef = ref, subRef]() noexcept {
                  auto ret = this->repo.remove(layerRef, layerInfo.packageInfoV2Module, subRef);
                  if (!ret) {
                      qCritical() << "rollback importLayerDir failed:" << ret.error().message();
                  }
              });
        }

        if (oldAppRef) {
            auto ret = removeAfterInstall(*oldAppRef, this->repo.getModuleList(*oldAppRef));
            if (!ret) {
                qCritical() << "remove old reference after install newer version failed:"
                            << ret.error().message();
            }
        }

        transaction.commit();
        this->repo.exportReference(appRef);

        auto mergeRet = this->repo.mergeModules();
        if (!mergeRet.has_value()) {
            qCritical() << "merge modules failed: " << mergeRet.error().message();
        }

        taskRef.updateState(linglong::api::types::v1::State::Succeed, "install uab successfully");
    };

    taskRef.setJob(std::move(installer));
    Q_EMIT TaskListChanged(taskRef.taskObjectPath());
    return utils::serialize::toQVariantMap(api::types::v1::PackageManager1PackageTaskResult{
      .taskObjectPath = taskRef.taskObjectPath().toStdString(),
      .code = 0,
      .message = (realFile + " is now installing").toStdString(),
    });
}

auto PackageManager::InstallFromFile(const QDBusUnixFileDescriptor &fd,
                                     const QString &fileType,
                                     const QVariantMap &options) noexcept -> QVariantMap
{
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
        return toDBusReply(QDBusError::NotSupported,
                           QString{ "%1 is unsupported fileType" }.arg(fileType));
    }

    return std::invoke(installers[fileType], this, fd, *opts);
}

auto PackageManager::Install(const QVariantMap &parameters) noexcept -> QVariantMap
{
    auto paras =
      utils::serialize::fromQVariantMap<api::types::v1::PackageManager1InstallParameters>(
        parameters);
    if (!paras) {
        return toDBusReply(paras);
    }

    auto fuzzyRef = fuzzyReferenceFromPackage(paras->package);
    if (!fuzzyRef) {
        return toDBusReply(fuzzyRef);
    }

    if (fuzzyRef->version) {
        auto ref = this->repo.clearReference(*fuzzyRef,
                                             {
                                               .fallbackToRemote = false // NOLINT
                                             });
        if (ref) {
            return toDBusReply(-1, ref->toString() + " is already installed.");
        }
    }

    api::types::v1::PackageManager1RequestInteractionAdditionalMessage additionalMessage;
    auto curModule = paras->package.packageManager1PackageModule.value_or("binary");

    // we need latest local reference
    std::optional<package::Version> version = fuzzyRef->version;
    fuzzyRef->version.reset();
    auto localRef = this->repo.clearReference(*fuzzyRef,
                                              {
                                                .fallbackToRemote = false // NOLINT
                                              });
    // set version back
    fuzzyRef->version = version;
    if (localRef) {
        // fallback to local version if version of fuzzyRef is not set
        if (curModule != "binary" && !fuzzyRef->version) {
            fuzzyRef->version = localRef->version;
        }

        additionalMessage.localRef = localRef->toString().toStdString();
    }

    auto remoteRefRet = this->repo.clearReference(*fuzzyRef,
                                                  {
                                                    .forceRemote = true // NOLINT
                                                  },
                                                  curModule);
    if (!remoteRefRet) {
        return toDBusReply(remoteRefRet);
    }
    auto remoteRef = *remoteRefRet;
    additionalMessage.remoteRef = remoteRef.toString().toStdString();

    // 安装模块之前要先安装binary
    if (curModule != "binary" && curModule != "runtime") {
        auto layerDir = this->repo.getLayerDir(remoteRef, "binary");
        if (!layerDir.has_value() || !layerDir->valid()) {
            return toDBusReply(-1, "to install the module, one must first install the binary");
        }
    }

    auto msgType = api::types::v1::InteractionMessageType::Install;
    if (!additionalMessage.localRef.empty()) {
        if (remoteRef.version == localRef->version) {
            return toDBusReply(-1, localRef->toString() + " is already installed");
        }

        if (remoteRef.version > localRef->version) {
            msgType = api::types::v1::InteractionMessageType::Upgrade;
        } else if (!paras->options.force) {
            return toDBusReply(-1,
                               "The latest version has been installed. If you need to "
                               "overwrite it, try using '--force'.");
        }
    }

    auto refSpec =
      QString{ "%1:%2/%3" }.arg(QString::fromStdString(this->repo.getConfig().defaultRepo),
                                remoteRef.toString(),
                                QString::fromStdString(curModule));
    auto task = std::find_if(this->taskList.cbegin(),
                             this->taskList.cend(),
                             [&refSpec](const PackageTask *task) {
                                 return task->isRefExist(refSpec);
                             });
    if (task != this->taskList.cend()) {
        return toDBusReply(-1, "the target " % refSpec % " is being operated");
    }

    auto *taskPtr = new PackageTask{ connection(), QStringList{ refSpec } };
    auto &taskRef = *(this->taskList.emplace_back(taskPtr));
    bool skipInteraction = paras->options.skipInteraction;

    // Note: do not capture any reference of variable which defined in this func.
    // it will be a dangling reference.
    taskRef.setJob([this,
                    &taskRef,
                    remoteRef,
                    localRef = localRef.has_value()
                      ? std::make_optional(std::move(localRef).value())
                      : std::nullopt,
                    curModule,
                    skipInteraction,
                    msgType,
                    additionalMessage]() {
        if (msgType == api::types::v1::InteractionMessageType::Upgrade && !skipInteraction) {
            Q_EMIT RequestInteraction(QDBusObjectPath(taskRef.taskObjectPath()),
                                      static_cast<int>(msgType),
                                      utils::serialize::toQVariantMap(additionalMessage));

            QEventLoop loop;
            api::types::v1::InteractionReply interactionReply;
            // Note: if capture the &taskRef into this lambda, be careful with it's life cycle.
            connect(this,
                    &PackageManager::ReplyReceived,
                    [&interactionReply, &loop](const QVariantMap &reply) {
                        interactionReply =
                          *utils::serialize::fromQVariantMap<api::types::v1::InteractionReply>(
                            reply);
                        loop.exit(0);
                    });
            loop.exec();
            if (interactionReply.action != "yes") {
                taskRef.updateState(linglong::api::types::v1::State::Canceled, "canceled");
            }
        }

        if (isTaskDone(taskRef.subState())) {
            return;
        }

        this->Install(taskRef, remoteRef, localRef, std::vector{ curModule });
    });

    // notify task list change
    Q_EMIT TaskListChanged(taskRef.taskObjectPath());
    qDebug() << "current task queue size:" << this->taskList.size();

    return utils::serialize::toQVariantMap(api::types::v1::PackageManager1PackageTaskResult{
      .taskObjectPath = taskRef.taskObjectPath().toStdString(),
      .code = 0,
      .message = (remoteRef.toString() + " is now installing").toStdString(),
    });
}

void PackageManager::Install(PackageTask &taskContext,
                             const package::Reference &newRef,
                             std::optional<package::Reference> oldRef,
                             const std::vector<std::string> &modules) noexcept
{
    taskContext.updateState(linglong::api::types::v1::State::Processing,
                            "Installing " + newRef.toString());

    utils::Transaction transaction;

    InstallRef(taskContext, newRef, modules);
    if (isTaskDone(taskContext.subState())) {
        return;
    }

    transaction.addRollBack([this, &newRef, &modules]() noexcept {
        auto tmp = PackageTask::createTemporaryTask();
        UninstallRef(tmp, newRef, modules);
        if (tmp.state() != linglong::api::types::v1::State::Succeed) {
            qCritical() << "failed to rollback install " << newRef.toString();
        }
    });

    taskContext.updateSubState(linglong::api::types::v1::SubState::PostAction,
                               "processing after install");

    if (oldRef) {
        auto ret = this->removeAfterInstall(*oldRef, this->repo.getModuleList(*oldRef));
        if (!ret) {
            taskContext.updateState(linglong::api::types::v1::State::Failed,
                                    "Failed to remove old reference " % oldRef->toString()
                                      % " after install " % newRef.toString());
            return;
        }
    }

    this->repo.exportReference(newRef);
    transaction.commit();

    taskContext.updateState(linglong::api::types::v1::State::Succeed,
                            "Install " + newRef.toString() + " success");

    auto mergeRet = this->repo.mergeModules();
    if (!mergeRet.has_value()) {
        qCritical() << "merge modules failed: " << mergeRet.error().message();
    }
}

void PackageManager::InstallRef(PackageTask &taskContext,
                                const package::Reference &ref,
                                std::vector<std::string> modules) noexcept
{
    LINGLONG_TRACE("install " + ref.toString());

    taskContext.updateSubState(linglong::api::types::v1::SubState::PreAction,
                               "Beginning to install");
    auto currentArch = package::Architecture::currentCPUArchitecture();
    if (!currentArch) {
        taskContext.updateState(linglong::api::types::v1::State::Failed,
                                currentArch.error().message());
    }

    if (ref.arch != *currentArch) {
        taskContext.updateState(linglong::api::types::v1::State::Failed,
                                "app arch:" + ref.arch.toString() + " not match host architecture");
    }

    taskContext.updateSubState(linglong::api::types::v1::SubState::InstallApplication,
                               "Installing application " + ref.toString());

    auto deletedList = this->repo.listLocalBy(
      linglong::repo::repoCacheQuery{ .id = ref.id.toStdString(),
                                      .channel = ref.channel.toStdString(),
                                      .version = ref.version.toString().toStdString(),
                                      .deleted = true });
    if (!deletedList) {
        taskContext.updateState(linglong::api::types::v1::State::Failed,
                                deletedList.error().message());
        Q_ASSERT(false);
        return;
    }

    utils::Transaction t;

    for (const auto &deletedItem : *deletedList) {
        if (isTaskDone(taskContext.subState())) {
            return;
        }

        auto it =
          std::find_if(modules.begin(), modules.end(), [&deletedItem](const std::string &module) {
              if (module == "runtime" && deletedItem.info.packageInfoV2Module == "binary") {
                  return true;
              }

              if (module == "binary" && deletedItem.info.packageInfoV2Module == "runtime") {
                  return true;
              }

              return module == deletedItem.info.packageInfoV2Module;
          });
        if (it == modules.end()) {
            continue;
        }

        auto ret = this->repo.markDeleted(ref, false, deletedItem.info.packageInfoV2Module);
        if (!ret) {
            qCritical() << "Failed to mark old package as deleted" << ref.toString() << ":"
                        << ret.error().message();
            taskContext.updateState(linglong::api::types::v1::State::Failed, "install failed");
            Q_ASSERT(false);
        }

        t.addRollBack([this, &ref, module = deletedItem.info.packageInfoV2Module]() noexcept {
            auto ret = this->repo.markDeleted(ref, true, module);
            if (!ret) {
                qWarning() << "failed to rollback marking deleted" << ref.toString() << ":"
                           << ret.error().message();
            }
        });

        modules.erase(it);
    }

    for (const auto &module : modules) {
        if (isTaskDone(taskContext.subState())) {
            return;
        }

        this->repo.pull(taskContext, ref, module);
        if (isTaskDone(taskContext.subState())) {
            return;
        }

        t.addRollBack([this, &ref, &module]() noexcept {
            auto result = this->repo.remove(ref, module);
            if (!result) {
                qCritical() << result.error();
                Q_ASSERT(false);
            }
        });

        if (module != "binary" && module != "runtime") {
            continue;
        }

        auto layerDir = this->repo.getLayerDir(ref);
        if (!layerDir) {
            taskContext.updateState(linglong::api::types::v1::State::Failed,
                                    LINGLONG_ERRV(layerDir).message());
            return;
        }

        auto info = layerDir->info();
        if (!info) {
            taskContext.updateState(linglong::api::types::v1::State::Failed,
                                    LINGLONG_ERRV(info).message());
            return;
        }

        // Note: Do not set module by app's module here
        pullDependency(taskContext, *info, "binary");
    }

    t.commit();
}

auto PackageManager::Uninstall(const QVariantMap &parameters) noexcept -> QVariantMap
{
    auto paras =
      utils::serialize::fromQVariantMap<api::types::v1::PackageManager1UninstallParameters>(
        parameters);
    if (!paras) {
        return toDBusReply(paras);
    }

    auto fuzzyRef = fuzzyReferenceFromPackage(paras->package);
    if (!fuzzyRef) {
        return toDBusReply(fuzzyRef);
    }

    auto ref = this->repo.clearReference(*fuzzyRef,
                                         {
                                           .fallbackToRemote = false // NOLINT
                                         });
    if (!ref) {
        return toDBusReply(-1, fuzzyRef->toString() + " not installed.");
    }
    auto reference = *ref;

    auto runningRef = isRefBusy(reference);
    if (!runningRef) {
        return toDBusReply(-1, "failed to get the state of target ref:" % reference.toString());
    }

    if (*runningRef) {
        return toDBusReply(-1,
                           "The application is currently running and cannot be "
                           "uninstalled. Please turn off the application and try again.",
                           "notification");
    }

    auto curModule = paras->package.packageManager1PackageModule.value_or("binary");
    auto refSpec =
      QString{ "%1/%2/%3" }.arg(QString::fromStdString(this->repo.getConfig().defaultRepo),
                                reference.toString(),
                                QString::fromStdString(curModule));
    auto task = std::find_if(this->taskList.cbegin(),
                             this->taskList.cend(),
                             [&refSpec](const PackageTask *task) {
                                 return task->isRefExist(refSpec);
                             });
    if (task != this->taskList.cend()) {
        return toDBusReply(-1, "the target " % refSpec % " is being operated");
    }
    auto *taskPtr = new PackageTask{ connection(), QStringList{ refSpec } };
    auto &taskRef = *(this->taskList.emplace_back(taskPtr));

    taskRef.setJob([this, &taskRef, reference, curModule]() {
        if (isTaskDone(taskRef.subState())) {
            return;
        }

        this->Uninstall(taskRef, reference, curModule);
    });
    // notify task list change
    Q_EMIT TaskListChanged(taskRef.taskObjectPath());
    qDebug() << "current task queue size:" << this->taskList.size();
    taskPtr->updateState(api::types::v1::State::Queued, "add uninstall task to task queue.");
    return utils::serialize::toQVariantMap(api::types::v1::PackageManager1PackageTaskResult{
      .taskObjectPath = taskRef.taskObjectPath().toStdString(),
      .code = 0,
      .message = (ref->toString() + " is now uninstalling").toStdString(),
    });
}

void PackageManager::UninstallRef(PackageTask &taskContext,
                                  const package::Reference &ref,
                                  const std::vector<std::string> &modules) noexcept
{
    LINGLONG_TRACE("uninstall ref " + ref.toString());
    if (isTaskDone(taskContext.subState())) {
        return;
    }

    taskContext.updateSubState(linglong::api::types::v1::SubState::Uninstall, "Remove layer files");
    utils::Transaction transaction;

    for (const auto &module : modules) {
        auto result = this->repo.remove(ref, module);
        if (!result) {
            taskContext.updateState(linglong::api::types::v1::State::Failed,
                                    LINGLONG_ERRV(result).message());
            return;
        }

        transaction.addRollBack([this, &ref, &module]() noexcept {
            auto tmpTask = PackageTask::createTemporaryTask();
            this->repo.pull(tmpTask, ref, module);
            if (tmpTask.state() != linglong::api::types::v1::State::Succeed) {
                qCritical() << "failed to rollback module" << module.c_str() << "of ref"
                            << ref.toString();
            }
        });
    }

    transaction.commit();
}

void PackageManager::Uninstall(PackageTask &taskContext,
                               const package::Reference &ref,
                               const std::string &module) noexcept
{
    if (isTaskDone(taskContext.subState())) {
        return;
    }

    taskContext.updateState(api::types::v1::State::Processing, "start to uninstalling package");
    taskContext.updateSubState(linglong::api::types::v1::SubState::PreAction,
                               "prepare uninstalling package");

    std::vector<std::string> removedModules{ "binary" };
    if (module == "binary") {
        auto modules = this->repo.getModuleList(ref);
        removedModules = std::move(modules);
    }

    utils::Transaction transaction;

    this->repo.unexportReference(ref);
    transaction.addRollBack([this, &ref]() noexcept {
        this->repo.exportReference(ref);
    });

    UninstallRef(taskContext, ref, removedModules);
    if (isTaskDone(taskContext.subState())) {
        return;
    }

    transaction.commit();

    taskContext.updateState(linglong::api::types::v1::State::Succeed,
                            "Uninstall " + ref.toString() + " success");

    auto mergeRet = this->repo.mergeModules();
    if (!mergeRet.has_value()) {
        qCritical() << "merge modules failed: " << mergeRet.error().message();
    }
}

utils::error::Result<package::Reference> PackageManager::latestRemoteReference(
  const std::string &kind, package::FuzzyReference &fuzzyRef) noexcept
{
    LINGLONG_TRACE("get latest reference");

    // Note: 应用更新策略与base/runtime不一致
    // 对于应用来说，不应该带着版本去查询, 允许从0.0.1更新到1.0.0
    // 对于base/runtime，应该带着版本去查询，只允许从0.0.1更新到0.0.2
    if (kind == "app") {
        fuzzyRef.version.reset();
        auto ref = this->repo.clearReference(fuzzyRef,
                                             {
                                               .forceRemote = true // NOLINT
                                             });
        if (!ref) {
            return LINGLONG_ERR(ref);
        }
        return ref;
    }
    auto ref = this->repo.clearReference(fuzzyRef,
                                         {
                                           .forceRemote = true // NOLINT
                                         });
    if (!ref) {
        return LINGLONG_ERR(ref);
    }
    return ref;
}

auto PackageManager::Update(const QVariantMap &parameters) noexcept -> QVariantMap
{
    auto paras = utils::serialize::fromQVariantMap<api::types::v1::PackageManager1UpdateParameters>(
      parameters);
    if (!paras) {
        return toDBusReply(paras);
    }

    QStringList refSpecList{};
    std::vector<std::pair<package::Reference, package::Reference>> upgradeList;

    for (const auto &package : paras->packages) {
        auto installedAppFuzzyRef = fuzzyReferenceFromPackage(package);
        if (!installedAppFuzzyRef) {
            return toDBusReply(installedAppFuzzyRef);
        }

        auto ref = this->repo.clearReference(*installedAppFuzzyRef,
                                             {
                                               .fallbackToRemote = false // NOLINT
                                             });
        if (!ref) {
            return toDBusReply(-1, installedAppFuzzyRef->toString() + " not installed.");
        }

        auto layerItem = this->repo.getLayerItem(*ref);
        if (!layerItem) {
            return toDBusReply(layerItem);
        }

        auto newRef = this->latestRemoteReference(layerItem->info.kind, *installedAppFuzzyRef);
        if (!newRef) {
            return toDBusReply(newRef);
        }

        if (newRef->version <= ref->version) {
            return toDBusReply(
              -1,
              QString("remote version is %1, the latest version %2 is already installed")
                .arg(newRef->version.toString())
                .arg(ref->version.toString()));
        }

        const auto reference = *ref;
        const auto newReference = *newRef;
        qInfo() << "Before upgrade, old Ref: " << reference.toString()
                << " new Ref: " << newReference.toString();

        // FIXME: use sha256 instead of refSpec
        auto curModule = package.packageManager1PackageModule.value_or("binary");
        auto refSpec =
          QString{ "%1/%2/%3" }.arg(QString::fromStdString(this->repo.getConfig().defaultRepo),
                                    newReference.toString(),
                                    QString::fromStdString(curModule));
        auto task = std::find_if(this->taskList.cbegin(),
                                 this->taskList.cend(),
                                 [&refSpec](const PackageTask *task) {
                                     return task->isRefExist(refSpec);
                                 });
        if (task != this->taskList.cend()) {
            return toDBusReply(-1, "the target " % refSpec % " is being operated");
        }
        refSpecList.append(refSpec);
        upgradeList.push_back(std::make_pair(reference, newReference));
    }

    auto *taskPtr = new PackageTask{ connection(), refSpecList };
    auto &taskRef = *(this->taskList.emplace_back(taskPtr));

    taskRef.setJob([this, &taskRef, upgradeList = std::move(upgradeList)]() {
        if (isTaskDone(taskRef.subState())) {
            return;
        }

        for (auto refs : upgradeList) {
            this->Update(taskRef, refs.first, refs.second);
        }
    });
    Q_EMIT TaskListChanged(taskRef.taskObjectPath());
    return utils::serialize::toQVariantMap(api::types::v1::PackageManager1PackageTaskResult{
      .taskObjectPath = taskRef.taskObjectPath().toStdString(),
      .code = 0,
      .message = "updating",
    });
}

void PackageManager::Update(PackageTask &taskContext,
                            const package::Reference &ref,
                            const package::Reference &newRef) noexcept
{
    LINGLONG_TRACE("update " + ref.toString());
    taskContext.updateState(api::types::v1::State::Processing, "start to uninstalling package");
    auto modules = this->repo.getModuleList(ref);

    this->InstallRef(taskContext, newRef, modules);
    if (isTaskDone(taskContext.subState())) {
        return;
    }

    this->repo.unexportReference(ref);
    this->repo.exportReference(newRef);

    taskContext.updateState(linglong::api::types::v1::State::PartCompleted,
                            "Upgrade " + ref.toString() + " to " + newRef.toString() + " success");

    // use setMessage and setSubState directly will not trigger signal
    taskContext.setSubState(linglong::api::types::v1::SubState::PackageManagerDone),
      taskContext.setMessage(
        "Please restart the application after saving the data to experience the new version.");

    // we don't need to set task state to failed after install newer version successfully
    auto ret = removeAfterInstall(ref, modules);
    if (!ret) {
        qCritical() << "get state of ref" << ref.toString() << "failed:" << ret.error().message();
        return;
    }

    auto mergeRet = this->repo.mergeModules();
    if (!mergeRet.has_value()) {
        qCritical() << "merge modules failed: " << mergeRet.error().message();
    }
}

auto PackageManager::Search(const QVariantMap &parameters) noexcept -> QVariantMap
{
    auto paras = utils::serialize::fromQVariantMap<api::types::v1::PackageManager1SearchParameters>(
      parameters);
    if (!paras) {
        return toDBusReply(paras);
    }

    auto fuzzyRef = package::FuzzyReference::parse(QString::fromStdString(paras->id));
    if (!fuzzyRef) {
        return toDBusReply(fuzzyRef);
    }
    auto jobID = QUuid::createUuid().toString();
    auto ref = *fuzzyRef;
    m_search_queue.runTask([this, jobID, ref]() {
        auto pkgInfos = this->repo.listRemote(ref);
        if (!pkgInfos.has_value()) {
            qWarning() << "list remote failed: " << pkgInfos.error().message();
            Q_EMIT this->SearchFinished(jobID, toDBusReply(pkgInfos));
            return;
        }
        auto result = api::types::v1::PackageManager1SearchResult{
            .packages = *pkgInfos,
            .code = 0,
            .message = "",
        };
        Q_EMIT this->SearchFinished(jobID, utils::serialize::toQVariantMap(result));
    });
    auto result = utils::serialize::toQVariantMap(api::types::v1::PackageManager1JobInfo{
      .id = jobID.toStdString(),
      .code = 0,
      .message = "",
    });
    return result;
}

QVariantMap PackageManager::Migrate() noexcept
{
    qDebug() << "migrate request from:" << message().service();

    auto *migrate = new linglong::service::Migrate(this);
    new linglong::adaptors::migrate::Migrate1(migrate);
    auto ret =
      utils::dbus::registerDBusObject(connection(), "/org/deepin/linglong/Migrate1", migrate);
    if (!ret) {
        return toDBusReply(ret);
    }

    QMetaObject::invokeMethod(
      QCoreApplication::instance(),
      [this, migrate]() {
          auto ret = this->repo.dispatchMigration();
          if (!ret) {
              Q_EMIT migrate->MigrateDone(ret.error().code(), ret.error().message());
          } else {
              Q_EMIT migrate->MigrateDone(0, "migrate successfully");
          }

          migrate->deleteLater();
      },
      Qt::QueuedConnection);

    return utils::serialize::toQVariantMap(api::types::v1::CommonResult{
      .code = 0,
      .message = "package manager is migrating data",
    });
}

void PackageManager::pullDependency(PackageTask &taskContext,
                                    const api::types::v1::PackageInfoV2 &info,
                                    const std::string &module) noexcept
{
    if (info.kind != "app") {
        return;
    }

    LINGLONG_TRACE("pull dependencies of " + QString::fromStdString(info.id));

    utils::Transaction transaction;
    if (info.runtime) {
        auto fuzzyRuntime = package::FuzzyReference::parse(QString::fromStdString(*info.runtime));
        if (!fuzzyRuntime) {
            taskContext.updateState(linglong::api::types::v1::State::Failed,
                                    LINGLONG_ERRV(fuzzyRuntime).message());
            return;
        }

        auto runtime = this->repo.clearReference(*fuzzyRuntime,
                                                 {
                                                   .forceRemote = false,
                                                   .fallbackToRemote = true,
                                                 });
        if (!runtime) {
            taskContext.updateState(linglong::api::types::v1::State::Failed,
                                    runtime.error().message());
            return;
        }

        taskContext.updateSubState(linglong::api::types::v1::SubState::InstallRuntime,
                                   "Installing runtime " + runtime->toString());
        // 如果runtime已存在，则直接使用, 否则从远程拉取
        auto runtimeLayerDir = repo.getLayerDir(*runtime);
        if (!runtimeLayerDir) {
            if (isTaskDone(taskContext.subState())) {
                return;
            }

            this->repo.pull(taskContext, *runtime, module);

            if (isTaskDone(taskContext.subState())) {
                return;
            }

            transaction.addRollBack([this, runtimeRef = *runtime, module]() noexcept {
                auto result = this->repo.remove(runtimeRef, module);
                if (!result) {
                    qCritical() << result.error();
                    Q_ASSERT(false);
                }
            });
        }
    }

    auto fuzzyBase = package::FuzzyReference::parse(QString::fromStdString(info.base));
    if (!fuzzyBase) {
        taskContext.updateState(linglong::api::types::v1::State::Failed,
                                LINGLONG_ERRV(fuzzyBase).message());
        return;
    }

    auto base = this->repo.clearReference(*fuzzyBase,
                                          {
                                            .forceRemote = false,
                                            .fallbackToRemote = true,
                                          });
    if (!base) {
        taskContext.updateState(linglong::api::types::v1::State::Failed,
                                LINGLONG_ERRV(base).message());
        return;
    }

    taskContext.updateSubState(linglong::api::types::v1::SubState::InstallBase,
                               "Installing base " + base->toString());
    // 如果base已存在，则直接使用, 否则从远程拉取
    auto baseLayerDir = repo.getLayerDir(*base, module);
    if (!baseLayerDir) {
        if (isTaskDone(taskContext.subState())) {
            return;
        }
        this->repo.pull(taskContext, *base, module);
        if (isTaskDone(taskContext.subState())) {
            return;
        }
    }

    transaction.commit();
}

auto PackageManager::Prune() noexcept -> QVariantMap
{
    auto jobID = QUuid::createUuid().toString();
    m_prune_queue.runTask([this, jobID]() {
        std::vector<api::types::v1::PackageInfoV2> pkgs;
        auto ret = Prune(pkgs);
        if (!ret.has_value()) {
            Q_EMIT this->PruneFinished(jobID, toDBusReply(ret));
            return;
        }
        auto result = api::types::v1::PackageManager1SearchResult{
            .packages = pkgs,
            .code = 0,
            .message = "",
        };
        Q_EMIT this->PruneFinished(jobID, utils::serialize::toQVariantMap(result));
    });
    auto result = utils::serialize::toQVariantMap(api::types::v1::PackageManager1JobInfo{
      .id = jobID.toStdString(),
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
            auto runtimeFuzzyRef =
              package::FuzzyReference::parse(QString::fromStdString(info.runtime.value()));
            if (!runtimeFuzzyRef) {
                qWarning() << runtimeFuzzyRef.error().message();
                continue;
            }

            auto runtimeRef = this->repo.clearReference(*runtimeFuzzyRef,
                                                        {
                                                          .forceRemote = false,
                                                          .fallbackToRemote = false,
                                                        });
            if (!runtimeRef) {
                qWarning() << runtimeRef.error().message();
                continue;
            }
            target[*runtimeRef] += 1;
        }

        auto baseFuzzyRef = package::FuzzyReference::parse(QString::fromStdString(info.base));
        if (!baseFuzzyRef) {
            qWarning() << baseFuzzyRef.error().message();
            continue;
        }

        auto baseRef = this->repo.clearReference(*baseFuzzyRef,
                                                 {
                                                   .forceRemote = false,
                                                   .fallbackToRemote = false,
                                                 });
        if (!baseRef) {
            qWarning() << baseRef.error().message();
            continue;
        }
        target[*baseRef] += 1;
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

    auto mergeRet = this->repo.mergeModules();
    if (!mergeRet.has_value()) {
        qCritical() << "merge modules failed: " << mergeRet.error().message();
    }
    return LINGLONG_OK;
}

void PackageManager::ReplyInteraction(QDBusObjectPath object_path, const QVariantMap &replies)
{
    Q_EMIT this->ReplyReceived(replies);
}
} // namespace linglong::service
