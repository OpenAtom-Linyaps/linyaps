/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "package_manager.h"

#include "linglong/api/types/v1/Generators.hpp"
#include "linglong/api/types/v1/PackageManager1ModifyRepoParameters.hpp"
#include "linglong/package/layer_file.h"
#include "linglong/package/layer_packager.h"
#include "linglong/utils/finally/finally.h"
#include "linglong/utils/serialize/json.h"
#include "linglong/utils/transaction.h"

#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusUnixFileDescriptor>
#include <QDebug>
#include <QEventLoop>
#include <QJsonArray>
#include <QMetaObject>
#include <QSettings>

namespace linglong::service {

namespace {
template<typename T>
QVariantMap toDBusReply(const utils::error::Result<T> &x) noexcept
{
    Q_ASSERT(!x.has_value());

    return utils::serialize::toQVariantMap(api::types::v1::CommonResult{
      .code = x.error().code(),                    // NOLINT
      .message = x.error().message().toStdString() // NOLINT
    });
}

QVariantMap toDBusReply(int code, const QString &message) noexcept
{
    return utils::serialize::toQVariantMap(api::types::v1::CommonResult{
      .code = code,                    // NOLINT
      .message = message.toStdString() // NOLINT
    });
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
}

auto PackageManager::getConfiguration() const noexcept -> QVariantMap
{
    return utils::serialize::toQVariantMap(this->repo.getConfig());
}

auto PackageManager::setConfiguration(const QVariantMap &parameters) noexcept -> QVariantMap
{
    auto cfg = utils::serialize::fromQVariantMap<api::types::v1::RepoConfig>(parameters);
    if (!cfg) {
        return toDBusReply(cfg);
    }

    auto result = this->repo.setConfig(*cfg);
    if (!result) {
        return toDBusReply(result);
    }

    return toDBusReply(0, "Set repository configuration success.");
}

auto PackageManager::InstallLayer(const QDBusUnixFileDescriptor &fd) noexcept -> QVariantMap
{
    const auto layerFile =
      package::LayerFile::New(QString("/proc/%1/fd/%2").arg(getpid()).arg(fd.fileDescriptor()));
    if (!layerFile) {
        return toDBusReply(layerFile);
    }
    Q_ASSERT(*layerFile != nullptr);

    package::LayerPackager layerPackager;
    auto layerDir = layerPackager.unpack(**layerFile);
    if (!layerDir) {
        return toDBusReply(layerDir);
    }

    auto result = this->repo.importLayerDir(*layerDir);
    if (!result) {
        return toDBusReply(result);
    }

    auto info = (*layerDir).info();
    if (!info) {
        return toDBusReply(info);
    }

    auto ref = package::Reference::fromPackageInfo(*info);
    if (!ref) {
        return toDBusReply(ref);
    }
    this->repo.exportReference(*ref);
    return toDBusReply(0, "Install layer file success.");
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

    auto ref = this->repo.clearReference(*fuzzyRef,
                                         {
                                           .fallbackToRemote = false // NOLINT
                                         });
    auto develop = paras->package.packageManager1PackageModule.value_or("runtime") == "develop";

    if (ref) {
        auto layerDir = this->repo.getLayerDir(*ref, develop);
        if (layerDir) {
            return toDBusReply(-1, ref->toString() + " is already installed");
        }
    }

    ref = this->repo.clearReference(*fuzzyRef,
                                    {
                                      .forceRemote = true // NOLINT
                                    });
    if (!ref) {
        return toDBusReply(ref);
    }

    if (taskMap.find(ref->toString()) != taskMap.cend()) {
        return toDBusReply(-1, ref->toString() + " is installing");
    }

    auto taskID = QUuid::createUuid();
    auto taskPtr = std::make_shared<InstallTask>(taskID);
    connect(taskPtr.get(), &InstallTask::TaskChanged, this, &PackageManager::TaskChanged);

    auto reference = *ref;

    QMetaObject::invokeMethod(
      QCoreApplication::instance(),
      [this, reference, taskPtr, develop] {
          auto _ = utils::finally::finally([this, reference]() {
              this->taskMap.erase(reference.toString());
          });

          this->Install(taskPtr, reference, develop);
      },
      Qt::QueuedConnection);

    // FIXME: Install task contains module, we should not just use ref as key.
    taskMap.emplace(ref->toString(), std::move(taskPtr));

    return utils::serialize::toQVariantMap(api::types::v1::PackageManager1ResultWithTaskID{
      .taskID = taskID.toString(QUuid::WithoutBraces).toStdString(),
      .code = 0,
      .message = (ref->toString() + " is now installing").toStdString(),
    });
}

void PackageManager::Install(const std::shared_ptr<InstallTask> &taskContext,
                             const package::Reference &ref,
                             bool develop) noexcept
{
    LINGLONG_TRACE("install " + ref.toString());

    taskContext->updateStatus(InstallTask::preInstall, "prepare installing " + ref.toString());

    auto currentArch = package::Architecture::parse(QSysInfo::currentCpuArchitecture());
    Q_ASSERT(currentArch.has_value());
    if (ref.arch != *currentArch) {
        taskContext->updateStatus(InstallTask::Failed,
                                  "app arch:" + ref.arch.toString()
                                    + " not match host architecture");
        taskMap.erase(ref.toString());
        return;
    }

    utils::Transaction t;

    this->repo.pull(taskContext, ref, develop);
    if (taskContext->currentStatus() == InstallTask::Failed
        || taskContext->currentStatus() == InstallTask::Canceled) {
        return;
    }
    t.addRollBack([this, &ref, develop]() noexcept {
        auto result = this->repo.remove(ref, develop);
        if (!result) {
            qCritical() << result.error();
            Q_ASSERT(false);
        }
    });

    auto layerDir = this->repo.getLayerDir(ref);
    if (!layerDir) {
        taskContext->updateStatus(InstallTask::Failed, LINGLONG_ERRV(layerDir).message());
        return;
    }

    auto info = layerDir->info();
    if (!info) {
        taskContext->updateStatus(InstallTask::Failed, LINGLONG_ERRV(info).message());
        return;
    }
    // for 'kind: app', check runtime and foundation
    if (info->kind == "app") {
        if (info->runtime) {
            auto fuzzyRuntime =
              package::FuzzyReference::parse(QString::fromStdString(*info->runtime));
            if (!fuzzyRuntime) {
                taskContext->updateStatus(InstallTask::Failed,
                                          LINGLONG_ERRV(fuzzyRuntime).message());
                return;
            }

            auto runtime = this->repo.clearReference(*fuzzyRuntime,
                                                     {
                                                       .forceRemote = true // NOLINT
                                                     });
            if (!runtime) {
                taskContext->updateStatus(InstallTask::Failed, runtime.error().message());
                return;
            }

            taskContext->updateStatus(InstallTask::installRuntime,
                                      "Installing runtime " + runtime->toString());
            this->repo.pull(taskContext, *runtime, develop);
            if (taskContext->currentStatus() == InstallTask::Failed
                || taskContext->currentStatus() == InstallTask::Canceled) {
                return;
            }

            auto runtimeRef = *runtime;
            t.addRollBack([this, runtimeRef, develop]() noexcept {
                auto result = this->repo.remove(runtimeRef, develop);
                if (!result) {
                    qCritical() << result.error();
                    Q_ASSERT(false);
                }
            });
        }

        auto fuzzyBase = package::FuzzyReference::parse(QString::fromStdString(info->base));
        if (!fuzzyBase) {
            taskContext->updateStatus(InstallTask::Failed, LINGLONG_ERRV(info).message());
            return;
        }

        auto base = this->repo.clearReference(*fuzzyBase,
                                              {
                                                .forceRemote = true // NOLINT
                                              });
        if (!base) {
            taskContext->updateStatus(InstallTask::Failed, LINGLONG_ERRV(base).message());
            return;
        }

        taskContext->updateStatus(InstallTask::installBase, "Installing base " + base->toString());
        this->repo.pull(taskContext, *base, develop);
        if (taskContext->currentStatus() == InstallTask::Failed
            || taskContext->currentStatus() == InstallTask::Canceled) {
            return;
        }
    }

    bool shouldExport = true;

    [&ref, this, &shouldExport]() {
        // Check if we should export the application we just pulled to system.

        auto pkgInfos = this->repo.listLocal();
        if (!pkgInfos) {
            qCritical() << pkgInfos.error();
            Q_ASSERT(false);
            return;
        }

        std::vector<package::Reference> refs;

        for (const auto &localInfo : *pkgInfos) {
            if (QString::fromStdString(localInfo.appid) != ref.id) {
                continue;
            }

            auto localRef = package::Reference::fromPackageInfo(localInfo);
            if (!localRef) {
                qCritical() << localRef.error();
                Q_ASSERT(false);
                continue;
            }

            if (localRef->version > ref.version) {
                qInfo() << localRef->toString() << "exists, we should not export" << ref.toString();
                shouldExport = false;
                return;
            }

            refs.push_back(*localRef);
        }

        for (const auto &ref : refs) {
            this->repo.unexportReference(ref);
        }
    }();

    if (shouldExport) {
        this->repo.exportReference(ref);
    }

    taskContext->updateStatus(InstallTask::Success, "Install " + ref.toString() + " success");
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

    auto develop = paras->package.packageManager1PackageModule.value_or("runtime") == "develop";

    auto result = this->repo.remove(*ref, develop);
    if (!result) {
        return toDBusReply(result);
    }

    this->repo.unexportReference(*ref);

    return toDBusReply(0, "Uninstall " + ref->toString() + " success.");
}

auto PackageManager::Update(const QVariantMap &parameters) noexcept -> QVariantMap
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

    auto fuzzyRefWithoutVersion = *fuzzyRef;
    fuzzyRefWithoutVersion.version = std::nullopt;

    auto newRef = this->repo.clearReference(*fuzzyRef,
                                            {
                                              .forceRemote = true // NOLINT
                                            });
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

    auto taskID = QUuid::createUuid();
    auto taskPtr = std::make_shared<InstallTask>(taskID);
    connect(taskPtr.get(), &InstallTask::TaskChanged, this, &PackageManager::TaskChanged);

    auto reference = *ref;
    auto newReference = *newRef;

    auto develop = paras->package.packageManager1PackageModule.value_or("runtime") == "develop";

    QMetaObject::invokeMethod(
      QCoreApplication::instance(),
      [this, reference, newReference, taskPtr, develop] {
          auto _ = utils::finally::finally([this, reference]() {
              this->taskMap.erase(reference.toString());
          });
          this->Update(taskPtr, reference, newReference, develop);
      },
      Qt::QueuedConnection);

    // FIXME(black_desk):
    // taskPtr is updating ref to newRef, but we just using ref as key. Does it really make sense?
    taskMap.emplace(ref->toString(), std::move(taskPtr));

    return utils::serialize::toQVariantMap(api::types::v1::PackageManager1ResultWithTaskID{
      .taskID = taskID.toString(QUuid::WithoutBraces).toStdString(),
      .code = 0,
      .message = (ref->toString() + " is updating").toStdString(),
    });
}

void PackageManager::Update(const std::shared_ptr<InstallTask> &taskContext,
                            const package::Reference &ref,
                            const package::Reference &newRef,
                            bool develop) noexcept
{
    LINGLONG_TRACE("update " + ref.toString());

    utils::Transaction t;

    this->Install(taskContext, newRef, develop);
    if (taskContext->currentStatus() == InstallTask::Failed
        || taskContext->currentStatus() == InstallTask::Canceled) {
        return;
    }
    t.addRollBack([this, &newRef, &develop]() noexcept {
        auto result = this->repo.remove(newRef, develop);
        if (!result) {
            qCritical() << result.error();
        }
        this->repo.unexportReference(newRef);
    });

    auto result = this->repo.remove(ref, develop);
    if (!result) {
        taskContext->updateStatus(InstallTask::Failed, result.error().message());
        return;
    }

    this->repo.unexportReference(ref);
    this->repo.exportReference(newRef);

    taskContext->updateStatus(InstallTask::Success, "Upgrade " + ref.toString() + " success");
    t.commit();
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

    auto pkgInfos = this->repo.listRemote(*fuzzyRef);
    if (!pkgInfos) {
        return toDBusReply(pkgInfos);
    }

    auto result = utils::serialize::toQVariantMap(api::types::v1::PackageManager1SearchResult{
      .packages = std::move(*pkgInfos),
      .code = 0,
      .message = "",
    });

    return result;
}

void PackageManager::CancelTask(const QString &taskID) noexcept
{
    auto task = std::find_if(taskMap.cbegin(), taskMap.cend(), [&taskID](const auto &task) {
        return task.second->taskID() == taskID;
    });

    if (task == taskMap.cend()) {
        return;
    }

    task->second->cancelTask();
    task->second->updateStatus(InstallTask::Canceled,
                               QString{ "cancel installing app 1" }.arg(task->first));
    taskMap.erase(task);
}

} // namespace linglong::service
