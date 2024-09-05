/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "package_manager.h"

#include "linglong/api/types/v1/Generators.hpp"
#include "linglong/package/layer_file.h"
#include "linglong/package/layer_packager.h"
#include "linglong/package/uab_file.h"
#include "linglong/utils/command/env.h"
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

auto PackageManager::UpdateConfiguration(uint16_t operation,
                                         const QString &repoName,
                                         const QString &url) noexcept -> QVariantMap
{
    LINGLONG_TRACE("update configuration")

    utils::error::Result<void> ret;
    if (repoName.isEmpty()) {
        ret = LINGLONG_ERR("repoName is an empty string");
        return toDBusReply(ret);
    }

    using Operation = linglong::api::dbus::v1::PackageManager::Operation;
    if (operation >= static_cast<int>(Operation::Unknown)) {
        ret = LINGLONG_ERR("repoName is an empty string");
        return toDBusReply(ret);
    }

    switch (static_cast<Operation>(operation)) {
    case api::dbus::v1::PackageManager::Operation::Add: {
        ret = this->repo.addRemoteRepo(repoName, url);
    } break;
    case api::dbus::v1::PackageManager::Operation::Remove: {
        ret = this->repo.removeRemoteRepo(repoName);
    } break;
    case api::dbus::v1::PackageManager::Operation::Update: {
        ret = this->repo.updateRemoteRepo(repoName, url);
    } break;
    case api::dbus::v1::PackageManager::Operation::Use: {
        ret = this->repo.setDefaultRemoteRepo(repoName);
    } break;
    case api::dbus::v1::PackageManager::Operation::Unknown:
        break;
    }

    if (!ret) {
        return toDBusReply(ret);
    }

    return toDBusReply(0, "Set repository configuration success.");
}

QVariantMap PackageManager::installFromLayer(const QDBusUnixFileDescriptor &fd) noexcept
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
    auto versionRet = package::Version::parse(QString::fromStdString(packageInfo.version));
    if (!versionRet) {
        return toDBusReply(versionRet);
    }

    auto fuzzyRef = package::FuzzyReference::parse(QString::fromStdString(packageInfo.id));
    if (!fuzzyRef) {
        return toDBusReply(fuzzyRef);
    }

    auto ref = this->repo.clearReference(*fuzzyRef,
                                         {
                                           .fallbackToRemote = false // NOLINT
                                         });
    if (ref) {
        auto layerDir = this->repo.getLayerDir(*ref, packageInfo.packageInfoV2Module);
        if (layerDir && layerDir->valid()) {
            return toDBusReply(-1, ref->toString() + " is already installed");
        }
    }

    auto architectureRet = package::Architecture::parse(packageInfo.arch[0]);
    if (!architectureRet) {
        return toDBusReply(architectureRet);
    }

    auto packageRefRet = package::Reference::create(QString::fromStdString(packageInfo.channel),
                                                    QString::fromStdString(packageInfo.id),
                                                    *versionRet,
                                                    *architectureRet);
    if (!packageRefRet) {
        return toDBusReply(packageRefRet);
    }
    const auto &packageRef = *packageRefRet;

    InstallTask task{ packageRef, packageInfo.packageInfoV2Module };
    if (std::find(this->taskList.cbegin(), this->taskList.cend(), task) != this->taskList.cend()) {
        return toDBusReply(-1,
                           "the target " % packageRef.toString() % "/"
                             % QString::fromStdString(packageInfo.packageInfoV2Module)
                             % " is being operated");
    }
    auto &taskRef = this->taskList.emplace_back(std::move(task));
    connect(&taskRef, &InstallTask::TaskChanged, this, &PackageManager::TaskChanged);

    auto installer =
      [this,
       fdDup = fd, // keep file descriptor don't close by the destructor of QDBusUnixFileDescriptor
       &taskRef,
       packageRef = std::move(packageRefRet).value(),
       layerFile = *layerFileRet,
       module = packageInfo.packageInfoV2Module]() {
          auto removeTask = utils::finally::finally([&taskRef, this] {
              auto elem = std::find(this->taskList.begin(), this->taskList.end(), taskRef);
              if (elem == this->taskList.end()) {
                  qCritical() << "the status of package manager is invalid";
                  return;
              }
              this->taskList.erase(elem);
          });
          taskRef.updateStatus(InstallTask::preInstall, "prepare for installing layer");

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
          if (taskRef.currentStatus() == InstallTask::Failed
              || taskRef.currentStatus() == InstallTask::Canceled) {
              return;
          }

          auto result = this->repo.importLayerDir(*layerDir);
          if (!result) {
              taskRef.reportError(std::move(result).error());
              return;
          }

          this->repo.exportReference(packageRef);
          taskRef.updateStatus(InstallTask::Success, "install layer successfully");
      };

    QMetaObject::invokeMethod(QCoreApplication::instance(),
                              std::move(installer),
                              Qt::QueuedConnection);

    return utils::serialize::toQVariantMap(api::types::v1::PackageManager1ResultWithTaskID{
      .taskID = taskRef.taskID().toStdString(),
      .code = 0,
      .message = (realFile + " is now installing").toStdString(),
    });
}

QVariantMap PackageManager::installFromUAB(const QDBusUnixFileDescriptor &fd) noexcept
{
    auto uabRet = package::UABFile::loadFromFile(
      QString("/proc/%1/fd/%2").arg(getpid()).arg(fd.fileDescriptor()));
    if (!uabRet) {
        return toDBusReply(uabRet);
    }
    const auto &uab = *uabRet;
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

    auto versionRet = package::Version::parse(QString::fromStdString(appLayer.info.version));
    if (!versionRet) {
        return toDBusReply(versionRet);
    }

    auto architectureRet = package::Architecture::parse(appLayer.info.arch[0]);
    if (!architectureRet) {
        return toDBusReply(architectureRet);
    }

    auto appRefRet = package::Reference::create(QString::fromStdString(appLayer.info.channel),
                                                QString::fromStdString(appLayer.info.id),
                                                *versionRet,
                                                *architectureRet);
    if (!appRefRet) {
        return toDBusReply(appRefRet);
    }
    const auto &appRef = *appRefRet;

    InstallTask task{ appRef, appLayer.info.packageInfoV2Module };
    if (std::find(this->taskList.cbegin(), this->taskList.cend(), task) != this->taskList.cend()) {
        return toDBusReply(-1,
                           "the target " % appRef.toString() % "/"
                             % QString::fromStdString(appLayer.info.packageInfoV2Module)
                             % " is being operated");
    }

    layerInfos.erase(appLayerIt);
    layerInfos.insert(layerInfos.begin(),
                      std::move(appLayer)); // app layer should place to the first of vector
    auto &taskRef = this->taskList.emplace_back(std::move(task));
    connect(&taskRef, &InstallTask::TaskChanged, this, &PackageManager::TaskChanged);

    auto installer =
      [this,
       &taskRef,
       fdDup = fd, // keep file descriptor don't close by the destructor of QDBusUnixFileDescriptor
       uab = std::move(uabRet).value(),
       layerInfos = std::move(layerInfos),
       metaInfo = std::move(metaInfoRet).value(),
       appRef = std::move(appRefRet).value()] {
          auto removeTask = utils::finally::finally([&taskRef, this] {
              auto elem = std::find(this->taskList.begin(), this->taskList.end(), taskRef);
              if (elem == this->taskList.end()) {
                  qCritical() << "the status of package manager is invalid";
                  return;
              }
              this->taskList.erase(elem);
          });

          if (taskRef.currentStatus() == InstallTask::Canceled) {
              qInfo() << "task" << taskRef.taskID() << "has been canceled by user, layer"
                      << taskRef.layer();
              return;
          }

          taskRef.updateStatus(InstallTask::preInstall, "prepare for installing uab");
          auto verifyRet = uab->verify();
          if (!verifyRet) {
              taskRef.reportError(std::move(verifyRet).error());
              return;
          }

          if (!*verifyRet) {
              taskRef.updateStatus(InstallTask::Failed, "couldn't pass uab verification");
              return;
          }

          if (taskRef.currentStatus() == InstallTask::Canceled) {
              qInfo() << "task" << taskRef.taskID() << "has been canceled by user, layer"
                      << taskRef.layer();
              return;
          }

          auto mountPoint = uab->mountUab();
          if (!mountPoint) {
              taskRef.reportError(std::move(mountPoint).error());
              return;
          }

          if (taskRef.currentStatus() == InstallTask::Canceled) {
              qInfo() << "task" << taskRef.taskID() << "has been canceled by user, layer"
                      << taskRef.layer();
              return;
          }

          const auto &uabLayersDirInfo = QFileInfo{ mountPoint->absoluteFilePath("layers") };
          if (!uabLayersDirInfo.exists() || !uabLayersDirInfo.isDir()) {
              taskRef.updateStatus(InstallTask::Failed,
                                   "the contents of this uab file are invalid");
              return;
          }

          utils::Transaction transaction;
          const auto &uabLayersDir = QDir{ uabLayersDirInfo.absoluteFilePath() };
          package::LayerDir appLayerDir;
          for (const auto &layer : layerInfos) {
              if (taskRef.currentStatus() == InstallTask::Canceled) {
                  qInfo() << "task" << taskRef.taskID() << "has been canceled by user, layer"
                          << taskRef.layer();
                  return;
              }

              QDir layerDirPath = uabLayersDir.absoluteFilePath(
                QString::fromStdString(layer.info.id) % QDir::separator()
                % QString::fromStdString(layer.info.packageInfoV2Module));

              if (!layerDirPath.exists()) {
                  taskRef.updateStatus(InstallTask::Failed,
                                       "layer directory " % layerDirPath.absolutePath()
                                         % " doesn't exist");
                  return;
              }

              const auto &layerDir = package::LayerDir{ layerDirPath.absolutePath() };
              std::string subRef;
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
                  subRef.clear();
              }

              auto ret = this->repo.importLayerDir(layerDir, subRef);
              if (!ret) {
                  if (ret.error().code() == 0
                      && !isAppLayer) { // if dependency already exist, skip it
                      continue;
                  }
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

          transaction.commit();
          this->repo.exportReference(appRef);

          taskRef.updateStatus(InstallTask::Success, "install uab successfully");
      };

    QMetaObject::invokeMethod(QCoreApplication::instance(),
                              std::move(installer),
                              Qt::QueuedConnection);

    return utils::serialize::toQVariantMap(api::types::v1::PackageManager1ResultWithTaskID{
      .taskID = taskRef.taskID().toStdString(),
      .code = 0,
      .message = (realFile + " is now installing").toStdString(),
    });
}

auto PackageManager::InstallFromFile(const QDBusUnixFileDescriptor &fd,
                                     const QString &fileType) noexcept -> QVariantMap
{
    const static QHash<QString,
                       QVariantMap (PackageManager::*)(const QDBusUnixFileDescriptor &) noexcept>
      installers = { { "layer", &PackageManager::installFromLayer },
                     { "uab", &PackageManager::installFromUAB } };

    if (!installers.contains(fileType)) {
        return toDBusReply(QDBusError::NotSupported,
                           QString{ "%1 is unsupported fileType" }.arg(fileType));
    }

    return std::invoke(installers[fileType], this, fd);
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
    auto curModule = paras->package.packageManager1PackageModule.value_or("runtime");

    if (ref) {
        auto layerDir = this->repo.getLayerDir(*ref, curModule);
        if (layerDir && layerDir->valid()) {
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
    auto reference = *ref;

    InstallTask task{ reference, curModule };
    if (std::find(this->taskList.cbegin(), this->taskList.cend(), task) != this->taskList.cend()) {
        return toDBusReply(-1,
                           "the target " % reference.toString() % "/"
                             % QString::fromStdString(curModule) % " is being operated");
    }

    auto &taskRef = this->taskList.emplace_back(std::move(task));
    connect(&taskRef, &InstallTask::TaskChanged, this, &PackageManager::TaskChanged);

    QMetaObject::invokeMethod(
      QCoreApplication::instance(),
      [this, reference, &taskRef, curModule] {
          auto _ = utils::finally::finally([this, reference, &taskRef]() {
              auto elem = std::find(this->taskList.begin(), this->taskList.end(), taskRef);
              if (elem == this->taskList.end()) {
                  qCritical() << "the status of package manager is invalid";
                  return;
              }
              this->taskList.erase(elem);
          });

          this->Install(taskRef, reference, curModule);
      },
      Qt::QueuedConnection);

    return utils::serialize::toQVariantMap(api::types::v1::PackageManager1ResultWithTaskID{
      .taskID = taskRef.taskID().toStdString(),
      .code = 0,
      .message = (ref->toString() + " is now installing").toStdString(),
    });
}

void PackageManager::Install(InstallTask &taskContext,
                             const package::Reference &ref,
                             const std::string &module) noexcept
{
    LINGLONG_TRACE("install " + ref.toString());

    taskContext.updateStatus(InstallTask::preInstall, "prepare installing " + ref.toString());

    auto currentArch = package::Architecture::currentCPUArchitecture();
    Q_ASSERT(currentArch.has_value());
    if (ref.arch != *currentArch) {
        taskContext.updateStatus(InstallTask::Failed,
                                 "app arch:" + ref.arch.toString()
                                   + " not match host architecture");
        return;
    }

    utils::Transaction t;

    this->repo.pull(taskContext, ref, module);
    if (taskContext.currentStatus() == InstallTask::Failed
        || taskContext.currentStatus() == InstallTask::Canceled) {
        return;
    }
    t.addRollBack([this, &ref, &module]() noexcept {
        auto result = this->repo.remove(ref, module);
        if (!result) {
            qCritical() << result.error();
            Q_ASSERT(false);
        }
    });

    auto layerDir = this->repo.getLayerDir(ref);
    if (!layerDir) {
        taskContext.updateStatus(InstallTask::Failed, LINGLONG_ERRV(layerDir).message());
        return;
    }

    auto info = layerDir->info();
    if (!info) {
        taskContext.updateStatus(InstallTask::Failed, LINGLONG_ERRV(info).message());
        return;
    }
    // for 'kind: app', check runtime and foundation
    if (info->kind == "app") {
        pullDependency(taskContext, *info, module);
    }

    // check the status of pull runtime and foundation
    if (taskContext.currentStatus() == InstallTask::Failed
        || taskContext.currentStatus() == InstallTask::Canceled) {
        return;
    }

    this->repo.exportReference(ref);

    taskContext.updateStatus(InstallTask::Success, "Install " + ref.toString() + " success");
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

    auto module = paras->package.packageManager1PackageModule.value_or("binary");
    this->repo.unexportReference(*ref);
    auto result = this->repo.remove(*ref, module);
    if (!result) {
        return toDBusReply(result);
    }

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

    auto installedAppFuzzyRef = fuzzyReferenceFromPackage(paras->package);
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

    auto newRef = this->repo.clearReference(*installedAppFuzzyRef,
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

    const auto reference = *ref;
    const auto newReference = *newRef;
    qInfo() << "Before upgrade, old Ref: " << reference.toString()
            << " new Ref: " << newReference.toString();

    auto curModule = paras->package.packageManager1PackageModule.value_or("runtime");

    InstallTask task{ newReference, curModule };
    if (std::find(this->taskList.cbegin(), this->taskList.cend(), task) != this->taskList.cend()) {
        return toDBusReply(-1,
                           "the target " % newReference.toString() % "/"
                             % QString::fromStdString(curModule) % " is being operated");
    }

    auto &taskRef = this->taskList.emplace_back(std::move(task));
    connect(&taskRef, &InstallTask::TaskChanged, this, &PackageManager::TaskChanged);

    QMetaObject::invokeMethod(
      QCoreApplication::instance(),
      [this, reference, newReference, &taskRef, curModule] {
          auto removeTask = utils::finally::finally([&taskRef, this] {
              auto elem = std::find(this->taskList.begin(), this->taskList.end(), taskRef);
              if (elem == this->taskList.end()) {
                  qCritical() << "the status of package manager is invalid";
                  return;
              }
              this->taskList.erase(elem);
          });

          this->Update(taskRef, reference, newReference, curModule);
      },
      Qt::QueuedConnection);

    return utils::serialize::toQVariantMap(api::types::v1::PackageManager1ResultWithTaskID{
      .taskID = taskRef.taskID().toStdString(),
      .code = 0,
      .message = (ref->toString() + " is updating").toStdString(),
    });
}

void PackageManager::Update(InstallTask &taskContext,
                            const package::Reference &ref,
                            const package::Reference &newRef,
                            const std::string &module) noexcept
{
    LINGLONG_TRACE("update " + ref.toString());

    utils::Transaction t;

    this->Install(taskContext, newRef, module);
    if (taskContext.currentStatus() == InstallTask::Failed
        || taskContext.currentStatus() == InstallTask::Canceled) {
        return;
    }
    t.addRollBack([this, &newRef, &ref, &module]() noexcept {
        auto result = this->repo.remove(newRef, module);
        if (!result) {
            qCritical() << result.error();
        }
        this->repo.unexportReference(newRef);
        this->repo.exportReference(ref);
    });

    this->repo.unexportReference(ref);
    this->repo.exportReference(newRef);

    taskContext.updateStatus(InstallTask::Success,
                             "Upgrade " + ref.toString() + "to" + newRef.toString() + " success");
    t.commit();

    // try to remove old version
    auto result = this->repo.remove(ref, module);
    if (!result) {
        qCritical() << "Failed to remove old package: " << ref.toString();
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
    auto task = std::find_if(taskList.begin(), taskList.end(), [&taskID](const InstallTask &task) {
        return task.taskID() == taskID;
    });

    if (task == taskList.cend()) {
        return;
    }

    task->cancelTask();
    task->updateStatus(InstallTask::Canceled,
                       QString{ "cancel installing app %1" }.arg(task->layer()));
}

void PackageManager::pullDependency(InstallTask &taskContext,
                                    const api::types::v1::PackageInfoV2 &info,
                                    const std::string &module) noexcept
{
    LINGLONG_TRACE("pull dependency runtime and base");

    utils::Transaction transaction;

    if (info.runtime) {
        auto fuzzyRuntime = package::FuzzyReference::parse(QString::fromStdString(*info.runtime));
        if (!fuzzyRuntime) {
            taskContext.updateStatus(InstallTask::Failed, LINGLONG_ERRV(fuzzyRuntime).message());
            return;
        }

        auto runtime = this->repo.clearReference(*fuzzyRuntime,
                                                 {
                                                   .forceRemote = true // NOLINT
                                                 });
        if (!runtime) {
            taskContext.updateStatus(InstallTask::Failed, runtime.error().message());
            return;
        }

        // 如果runtime已存在，则直接使用, 否则从远程拉取
        auto runtimeLayerDir = repo.getLayerDir(*runtime, module);
        if (!runtimeLayerDir) {
            taskContext.updateStatus(InstallTask::installRuntime,
                                     "Installing runtime " + runtime->toString());

            if (taskContext.currentStatus() == InstallTask::Canceled) {
                return;
            }

            this->repo.pull(taskContext, *runtime, module);

            if (taskContext.currentStatus() == InstallTask::Failed) {
                return;
            }

            auto runtimeRef = *runtime;
            transaction.addRollBack([this, runtimeRef, &module]() noexcept {
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
        taskContext.updateStatus(InstallTask::Failed, LINGLONG_ERRV(fuzzyBase).message());
        return;
    }

    auto base = this->repo.clearReference(*fuzzyBase,
                                          {
                                            .forceRemote = true // NOLINT
                                          });
    if (!base) {
        taskContext.updateStatus(InstallTask::Failed, LINGLONG_ERRV(base).message());
        return;
    }

    // 如果base已存在，则直接使用, 否则从远程拉取
    auto baseLayerDir = repo.getLayerDir(*base, module);
    if (!baseLayerDir) {
        taskContext.updateStatus(InstallTask::installBase, "Installing base " + base->toString());
        if (taskContext.currentStatus() == InstallTask::Canceled) {
            return;
        }

        this->repo.pull(taskContext, *base, module);
        if (taskContext.currentStatus() == InstallTask::Failed) {
            return;
        }
    }

    transaction.commit();
}

} // namespace linglong::service
