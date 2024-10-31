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
#include "linglong/package_manager/task.h"
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
#include <utility>

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
    // exec install on task list changed signal
    connect(
      this,
      &PackageManager::TaskListChanged,
      this,
      [this](const QString &taskID) {
          // notify task waiting
          if (!this->runningTaskID.isEmpty()) {
              for (auto &task : taskList) {
                  // skip tasks without job
                  if (!task.getJob().has_value() || task.taskID() == runningTaskID) {
                      continue;
                  }
                  auto msg = QString("Waiting for the other tasks");
                  task.updateState(PackageTask::Queued, msg);
              }
              return;
          }
          // start next task
          this->runningTaskID = taskID;
          if (this->taskList.empty()) {
              return;
          };
          for (auto task = taskList.begin(); task != taskList.end(); ++task) {
              if (!task->getJob().has_value() || task->state() != PackageTask::Queued) {
                  continue;
              }
              // execute the task
              auto func = *task->getJob();
              func();
              this->runningTaskID = "";
              taskList.erase(task);
              Q_EMIT this->TaskListChanged("");
              return;
          }
      },
      Qt::QueuedConnection);
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

    auto packageRefRet = package::Reference::create(QString::fromStdString(packageInfo.channel),
                                                    QString::fromStdString(packageInfo.id),
                                                    *versionRet,
                                                    *architectureRet);
    if (!packageRefRet) {
        return toDBusReply(packageRefRet);
    }
    const auto &packageRef = *packageRefRet;

    PackageTask task{ packageRef, packageInfo.packageInfoV2Module };
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
          taskRef.updateState(PackageTask::State::Installing, "installing layer");
          taskRef.updateSubState(PackageTask::SubState::PreAction, "preparing environment");

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
          if (taskRef.state() == PackageTask::Failed
              || taskRef.state() == PackageTask::Canceled) {
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
          taskRef.updateState(PackageTask::Succeed, "install layer successfully");
      };
    taskRef.setJob(std::move(installer));
    Q_EMIT TaskListChanged(taskRef.taskID());
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

    auto currentArch = package::Architecture::currentCPUArchitecture();
    if (!currentArch) {
        return toDBusReply(currentArch);
    }

    if (*architectureRet != *currentArch) {
        return toDBusReply(-1,
                           "app arch:" + architectureRet->toString()
                             + " not match host architecture");
    }

    auto appRefRet = package::Reference::create(QString::fromStdString(appLayer.info.channel),
                                                QString::fromStdString(appLayer.info.id),
                                                *versionRet,
                                                *architectureRet);
    if (!appRefRet) {
        return toDBusReply(appRefRet);
    }
    const auto &appRef = *appRefRet;

    PackageTask task{ appRef, appLayer.info.packageInfoV2Module };
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
          if (taskRef.currentStatus() == InstallTask::Canceled) {
              qInfo() << "task" << taskRef.taskID() << "has been canceled by user, layer"
                      << taskRef.layer();
              return;
          }

          taskRef.updateState(PackageTask::Installing, "installing uab");
          taskRef.updateSubState(PackageTask::PreAction, "prepare environment");
          auto verifyRet = uab->verify();
          if (!verifyRet) {
              taskRef.reportError(std::move(verifyRet).error());
              return;
          }

          if (!*verifyRet) {
              taskRef.updateState(PackageTask::Failed, "couldn't pass uab verification");
              return;
          }

          if (taskRef.state() == PackageTask::Canceled) {
              qInfo() << "task" << taskRef.taskID() << "has been canceled by user, layer"
                      << taskRef.layer();
              return;
          }

          auto mountPoint = uab->mountUab();
          if (!mountPoint) {
              taskRef.reportError(std::move(mountPoint).error());
              return;
          }

          if (taskRef.state() == PackageTask::Canceled) {
              qInfo() << "task" << taskRef.taskID() << "has been canceled by user, layer"
                      << taskRef.layer();
              return;
          }

          const auto &uabLayersDirInfo = QFileInfo{ mountPoint->absoluteFilePath("layers") };
          if (!uabLayersDirInfo.exists() || !uabLayersDirInfo.isDir()) {
              taskRef.updateState(PackageTask::Failed, "the contents of this uab file are invalid");
              return;
          }

          utils::Transaction transaction;
          const auto &uabLayersDir = QDir{ uabLayersDirInfo.absoluteFilePath() };
          package::LayerDir appLayerDir;
          for (const auto &layer : layerInfos) {
              if (taskRef.state() == PackageTask::Canceled) {
                  qInfo() << "task" << taskRef.taskID() << "has been canceled by user, layer"
                          << taskRef.layer();
                  return;
              }

              QDir layerDirPath = uabLayersDir.absoluteFilePath(
                QString::fromStdString(layer.info.id) % QDir::separator()
                % QString::fromStdString(layer.info.packageInfoV2Module));

              if (!layerDirPath.exists()) {
                  taskRef.updateState(PackageTask::Failed,
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

          auto mergeRet = this->repo.mergeModules();
          if (!mergeRet.has_value()) {
              qCritical() << "merge modules failed: " << mergeRet.error().message();
          }
          taskRef.updateState(PackageTask::Succeed, "install uab successfully");
      };

    taskRef.setJob(std::move(installer));
    Q_EMIT TaskListChanged(taskRef.taskID());
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
    // 判断应用是否已存在
    auto localRef = this->repo.clearReference(*fuzzyRef,
                                              {
                                                .fallbackToRemote = false // NOLINT
                                              });
    auto curModule = paras->package.packageManager1PackageModule.value_or("binary");
    if (localRef) {
        auto layerDir = this->repo.getLayerDir(*localRef, curModule);
        if (layerDir && layerDir->valid()) {
            return toDBusReply(-1, localRef->toString() + " is already installed");
        }
        // 如果要安装module，并且没有指定版本，应该使用已安装的binary的版本
        if (curModule != "binary" && !fuzzyRef->version.has_value()) {
            fuzzyRef->version = localRef->version;
        }
    }
    // 查询远程应用
    auto ref = this->repo.clearReference(*fuzzyRef,
                                         {
                                           .forceRemote = true // NOLINT
                                         },
                                         curModule);
    if (!ref) {
        return toDBusReply(ref);
    }
    auto reference = *ref;
    // 安装模块之前要先安装binary
    if (curModule != "binary") {
        auto layerDir = this->repo.getLayerDir(reference, "binary");
        if (!layerDir.has_value() || !layerDir->valid()) {
            return toDBusReply(-1, "to install the module, one must first install the binary");
        }
    }

    PackageTask task{ reference, curModule };
    if (std::find(this->taskList.cbegin(), this->taskList.cend(), task) != this->taskList.cend()) {
        return toDBusReply(-1,
                           "the target " % reference.toString() % "/"
                             % QString::fromStdString(curModule) % " is being operated");
    }

    // append to the task list
    auto &taskRef = this->taskList.emplace_back(std::move(task));
    connect(&taskRef, &InstallTask::TaskChanged, this, &PackageManager::TaskChanged);

    taskRef.setJob([this, &taskRef, reference, curModule]() {
        this->InstallRef(taskRef, reference, curModule);
        taskRef.updateStatus(InstallTask::Success, "Install " + reference.toString() + " success");
    });
    // notify task list change
    Q_EMIT TaskListChanged(taskRef.taskID());
    qWarning() << "current task queue size:" << this->taskList.size();
    return utils::serialize::toQVariantMap(api::types::v1::PackageManager1ResultWithTaskID{
      .taskID = taskRef.taskID().toStdString(),
      .code = 0,
      .message = (ref->toString() + " is now installing").toStdString(),
    });
}

void PackageManager::InstallRef(PackageTask &taskContext,
                                const package::Reference &ref,
                                const std::string &module) noexcept
{
    LINGLONG_TRACE("install " + ref.toString());

    taskContext.updateState(PackageTask::Installing, "installing " + ref.toString());
    taskContext.updateSubState(PackageTask::PreAction, "beginning to install");

    auto currentArch = package::Architecture::currentCPUArchitecture();
    if (!currentArch) {
        taskContext.updateStatus(InstallTask::Failed, currentArch.error().message());
        return;
    }

    if (ref.arch != *currentArch) {
        taskContext.updateState(PackageTask::Failed,
                                "app arch:" + ref.arch.toString() + " not match host architecture");
        return;
    }

    utils::Transaction t;

    this->repo.pull(taskContext, ref, develop);
    if (taskContext.state() == PackageTask::Failed
        || taskContext.state() == PackageTask::Canceled) {
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
        taskContext.updateState(PackageTask::Failed, LINGLONG_ERRV(layerDir).message());
        return;
    }

    auto info = layerDir->info();
    if (!info) {
        taskContext.updateState(PackageTask::Failed, LINGLONG_ERRV(info).message());
        return;
    }

    // for 'kind: app', check runtime and foundation
    if (info->kind == "app") {
        pullDependency(taskContext, *info, "binary");
    }

    // check the status of pull runtime and foundation
    if (taskContext.state() == PackageTask::Failed
        || taskContext.state() == PackageTask::Canceled) {
        return;
    }

    this->repo.exportReference(ref);

    auto mergeRet = this->repo.mergeModules();
    if (!mergeRet.has_value()) {
        qCritical() << "merge modules failed: " << mergeRet.error().message();
    }
    taskContext.updateState(PackageTask::Succeed, "Install " + ref.toString() + " success");
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
    // 卸载binary会同时卸载所有模块
    if (module == "binary") {
        auto modules = this->repo.getModuleList(*ref);
        for (const auto &mod : modules) {
            auto result = this->repo.remove(*ref, mod);
            if (!result) {
                return toDBusReply(result);
            }
        }
    } else {
        auto result = this->repo.remove(*ref, module);
        if (!result) {
            return toDBusReply(result);
        }
    }
    auto mergeRet = this->repo.mergeModules();
    if (!mergeRet.has_value()) {
        qCritical() << "merge modules failed: " << mergeRet.error().message();
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

    auto curModule = paras->package.packageManager1PackageModule.value_or("binary");
    PackageTask task{ newReference, curModule };
    if (std::find(this->taskList.cbegin(), this->taskList.cend(), task) != this->taskList.cend()) {
        return toDBusReply(-1,
                           "the target " % newReference.toString() % "/"
                             % QString::fromStdString(curModule) % " is being operated");
    }

    auto &taskRef = this->taskList.emplace_back(std::move(task));
    connect(&taskRef, &InstallTask::TaskChanged, this, &PackageManager::TaskChanged);
    taskRef.setJob([this, &taskRef, reference, newReference, curModule]() {
        this->Update(taskRef, reference, newReference);
    });
    Q_EMIT TaskListChanged(taskRef.taskID());

    return utils::serialize::toQVariantMap(api::types::v1::PackageManager1ResultWithTaskID{
      .taskID = taskRef.taskID().toStdString(),
      .code = 0,
      .message = (ref->toString() + " is updating").toStdString(),
    });
}

void PackageManager::Update(PackageTask &taskContext,
                            const package::Reference &ref,
                            const package::Reference &newRef) noexcept
{
    LINGLONG_TRACE("update " + ref.toString());
    auto modules = this->repo.getModuleList(ref);
    utils::Transaction t;
    t.addRollBack([this, &newRef, &modules]() noexcept {
        for (const auto &module : modules) {
            auto result = this->repo.remove(newRef, module);
            if (!result) {
                qCritical() << "Failed to remove new package: " << newRef.toString();
            }
        }
    });
    for (const auto &module : modules) {
        qDebug() << "update module:" << QString::fromStdString(module);
        this->InstallRef(taskContext, newRef, module);
        if (taskContext.state() == PackageTask::Failed
            || taskContext.state() == PackageTask::Canceled) {
            auto msg = QString("Upgrade %1/%3 to %2/%3 faield")
                         .arg(ref.toString())
                         .arg(newRef.toString())
                         .arg(module.c_str());
            taskContext.updateStatus(InstallTask::Failed, msg);
            return;
        }
    }
    t.addRollBack([this, &newRef, &ref]() noexcept {
        this->repo.unexportReference(newRef);
        this->repo.exportReference(ref);
    });
    this->repo.unexportReference(ref);
    this->repo.exportReference(newRef);

    taskContext.updateState(PackageTask::Succeed,
                            "Upgrade " + ref.toString() + "to" + newRef.toString() + " success");
    t.commit();

    // try to remove old version
    for (const auto &module : modules) {
        auto result = this->repo.remove(ref, module);
        if (!result) {
            qCritical() << "Failed to remove old package: " << ref.toString();
        }
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

    LINGLONG_TRACE("pull dependency runtime and base");

    utils::Transaction transaction;

    if (info.runtime) {
        auto fuzzyRuntime = package::FuzzyReference::parse(QString::fromStdString(*info.runtime));
        if (!fuzzyRuntime) {
            taskContext.updateState(PackageTask::Failed, LINGLONG_ERRV(fuzzyRuntime).message());
            return;
        }

        auto runtime = this->repo.clearReference(*fuzzyRuntime,
                                                 {
                                                   .forceRemote = false,
                                                   .fallbackToRemote = true,
                                                 });
        if (!runtime) {
            taskContext.updateState(PackageTask::Failed, runtime.error().message());
            return;
        }

        taskContext.updateSubState(PackageTask::InstallRuntime,
                                   "Installing runtime " + runtime->toString());

        // 如果runtime已存在，则直接使用, 否则从远程拉取
        auto runtimeLayerDir = repo.getLayerDir(*runtime, module);
        if (!runtimeLayerDir) {
            if (taskContext.state() == PackageTask::Canceled) {
                return;
            }

            this->repo.pull(taskContext, *runtime, module);

            if (taskContext.state() == PackageTask::Canceled
                || taskContext.state() == PackageTask::Failed) {
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
        taskContext.updateState(PackageTask::Failed, LINGLONG_ERRV(fuzzyBase).message());
        return;
    }

    auto base = this->repo.clearReference(*fuzzyBase,
                                          {
                                            .forceRemote = false,
                                            .fallbackToRemote = true,
                                          });
    if (!base) {
        taskContext.updateState(PackageTask::Failed, LINGLONG_ERRV(base).message());
        return;
    }

    taskContext.updateStatus(InstallTask::installBase, "Installing base " + base->toString());

    // 如果base已存在，则直接使用, 否则从远程拉取
    auto baseLayerDir = repo.getLayerDir(*base, module);
    if (!baseLayerDir) {
        taskContext.updateSubState(PackageTask::InstallBase, "Installing base " + base->toString());
        if (taskContext.state() == PackageTask::Canceled) {
            return;
        }

        this->repo.pull(taskContext, *base, module);

        if (taskContext.state() == PackageTask::Canceled
            || taskContext.state() == PackageTask::Failed) {
            return;
        }
    }

    transaction.commit();
}

} // namespace linglong::service
