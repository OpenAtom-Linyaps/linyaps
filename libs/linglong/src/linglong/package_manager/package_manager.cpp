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
              this->runningTaskObjectPath = "";
              Q_EMIT this->TaskRemoved(QDBusObjectPath{ task->taskObjectPath() },
                                       static_cast<int>(task->state()),
                                       static_cast<int>(task->subState()),
                                       task->message());
              this->taskList.erase(it);
              task->deleteLater();
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

    auto refSpec =
      QString{ "%1/%2/%3" }.arg("local",
                                packageRef.toString(),
                                QString::fromStdString(packageInfo.packageInfoV2Module));
    auto task = std::find_if(this->taskList.cbegin(),
                             this->taskList.cend(),
                             [&refSpec](const PackageTask *task) {
                                 return refSpec == task->refSpec();
                             });
    if (task != this->taskList.cend()) {
        return toDBusReply(-1, "the target " % refSpec % " is being operated");
    }
    auto *taskPtr = new PackageTask{ connection(), refSpec };
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
       additionalMessage]() {
          if (msgType == api::types::v1::InteractionMessageType::Upgrade
              && !options.skipInteraction) {
              Q_EMIT RequestInteraction(QDBusObjectPath(taskRef.taskObjectPath()),
                                        static_cast<int>(msgType),
                                        utils::serialize::toQVariantMap(additionalMessage));
              QEventLoop loop;
              connect(
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
          }
          if (taskRef.subState() == linglong::api::types::v1::SubState::Done) {
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
          if (taskRef.subState() == linglong::api::types::v1::SubState::Done) {
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

    auto localRef = this->repo.clearReference(*fuzzyRef,
                                              {
                                                .fallbackToRemote = false // NOLINT
                                              });
    if (localRef) {
        auto layerDir = this->repo.getLayerDir(*localRef, appLayer.info.packageInfoV2Module);
        if (layerDir && layerDir->valid()) {
            additionalMessage.localRef = localRef->toString().toStdString();
        }
    }

    if (!additionalMessage.localRef.empty()) {
        if (appRef.version == localRef->version) {
            return toDBusReply(-1, localRef->toString() + " is already installed");
        } else if (appRef.version > localRef->version) {
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
                                 return refSpec == task->refSpec();
                             });
    if (task != this->taskList.cend()) {
        return toDBusReply(-1, "the target " % refSpec % " is being operated");
    }
    auto *taskPtr = new PackageTask{ connection(), refSpec };
    auto &taskRef = *(this->taskList.emplace_back(taskPtr));

    layerInfos.erase(appLayerIt);
    layerInfos.insert(layerInfos.begin(),
                      std::move(appLayer)); // app layer should place to the first of vector

    auto installer =
      [this,
       &taskRef,
       fdDup = fd, // keep file descriptor don't close by the destructor of QDBusUnixFileDescriptor
       uab = std::move(uabRet).value(),
       layerInfos = std::move(layerInfos),
       metaInfo = std::move(metaInfoRet).value(),
       appRef = std::move(appRefRet).value(),
       options,
       msgType,
       additionalMessage] {
          if (msgType == api::types::v1::InteractionMessageType::Upgrade
              && !options.skipInteraction) {
              Q_EMIT RequestInteraction(QDBusObjectPath(taskRef.taskObjectPath()),
                                        static_cast<int>(msgType),
                                        utils::serialize::toQVariantMap(additionalMessage));
              QEventLoop loop;
              connect(
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
          }
          if (taskRef.subState() == linglong::api::types::v1::SubState::Done) {
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

          if (taskRef.subState() == linglong::api::types::v1::SubState::Done) {
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
              if (taskRef.subState() == linglong::api::types::v1::SubState::Done) {
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
              } else{
                  auto fuzzyString = refRet->id + "/" + refRet->version.toString();
                  auto fuzzyRef = package::FuzzyReference::parse(fuzzyString);
                  auto localRef = this->repo.clearReference(*fuzzyRef,
                                                            {
                                                              .fallbackToRemote = false // NOLINT
                                                            });
                  if (localRef) {
                      auto layerDir = this->repo.getLayerDir(*localRef, info.packageInfoV2Module);
                      if (layerDir && layerDir->valid() && refRet->version == localRef->version) {
                          continue;
                      }
                  }
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

    auto curModule = paras->package.packageManager1PackageModule.value_or("binary");
    auto remoteRef = this->repo.clearReference(*fuzzyRef,
                                               {
                                                 .forceRemote = true // NOLINT
                                               });
    if (!remoteRef) {
        return toDBusReply(remoteRef);
    }

    api::types::v1::PackageManager1RequestInteractionAdditonalMessage additionalMessage;
    api::types::v1::InteractionMessageType msgType =
      api::types::v1::InteractionMessageType::Install;

    additionalMessage.remoteRef = remoteRef->toString().toStdString();

    // Note: We can't clear the local Reference with a specific version.
    fuzzyRef->version.reset();
    auto localRef = this->repo.clearReference(*fuzzyRef,
                                              {
                                                .fallbackToRemote = false // NOLINT
                                              });
    if (localRef) {
        auto layerDir = this->repo.getLayerDir(*localRef, curModule);
        if (layerDir && layerDir->valid()) {
            additionalMessage.localRef = localRef->toString().toStdString();
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

    auto refSpec =
      QString{ "%1:%2/%3" }.arg(QString::fromStdString(this->repo.getConfig().defaultRepo),
                                reference.toString(),
                                QString::fromStdString(curModule));

    if (!additionalMessage.localRef.empty()) {
        if (remoteRef->version == localRef->version) {
            return toDBusReply(-1, localRef->toString() + " is already installed");
        } else if (remoteRef->version > localRef->version) {
            msgType = api::types::v1::InteractionMessageType::Upgrade;
        } else if (!paras->options.force) {
            return toDBusReply(-1,
                               "The latest version has been installed. If you need to "
                               "overwrite it, try using '--force'");
        }
    }

    auto refSpec =
      QString{ "%1/%2/%3" }.arg(QString::fromStdString(this->repo.getConfig().defaultRepo),
                                remoteRef->toString(),
                                QString::fromStdString(curModule));
    auto task = std::find_if(this->taskList.cbegin(),
                             this->taskList.cend(),
                             [&refSpec](const PackageTask *task) {
                                 return refSpec == task->refSpec();
                             });
    if (task != this->taskList.cend()) {
        return toDBusReply(-1, "the target " % refSpec % " is being operated");
    }

    auto *taskPtr = new PackageTask{ connection(), refSpec };
    auto &taskRef = *(this->taskList.emplace_back(taskPtr));
    auto reference = *remoteRef;
    bool skipInteraction = paras->options.skipInteraction;

    // Note: do not capture any reference of variable which defined in this func.
    // it will be a dangling reference.
    taskRef.setJob(
      [this, &taskRef, reference, curModule, skipInteraction, msgType, additionalMessage]() {
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

          if (taskRef.subState() == linglong::api::types::v1::SubState::Done) {
              return;
          }
          this->Install(taskRef, reference, curModule);
      });
    // notify task list change
    Q_EMIT TaskListChanged(taskRef.taskObjectPath());
    qDebug() << "current task queue size:" << this->taskList.size();
    return utils::serialize::toQVariantMap(api::types::v1::PackageManager1PackageTaskResult{
      .taskObjectPath = taskRef.taskObjectPath().toStdString(),
      .code = 0,
      .message = (remoteRef->toString() + " is now installing").toStdString(),
    });
}

void PackageManager::Install(PackageTask &taskContext,
                             const package::Reference &ref,
                             const std::string &module) noexcept
{
    taskContext.updateState(linglong::api::types::v1::State::Processing,
                            "Installing " + ref.toString());
    InstallRef(taskContext, ref, module);
    if (taskContext.subState() == linglong::api::types::v1::SubState::Done) {
        return;
    }

    taskContext.updateSubState(linglong::api::types::v1::SubState::PostAction,
                               "Export shared files");
    this->repo.exportReference(ref);
    taskContext.updateState(linglong::api::types::v1::State::Succeed,
                            "Install " + ref.toString() + " success");
}

void PackageManager::InstallRef(PackageTask &taskContext,
                                const package::Reference &ref,
                                const std::string &module) noexcept
{
    LINGLONG_TRACE("install " + ref.toString());

    taskContext.updateSubState(linglong::api::types::v1::SubState::PreAction,
                               "Beginning to install");
    auto currentArch = package::Architecture::currentCPUArchitecture();
    if (!currentArch) {
        taskContext.updateStatus(InstallTask::Failed, currentArch.error().message());
        return;
    }

    if (ref.arch != *currentArch) {
        taskContext.updateState(linglong::api::types::v1::State::Failed,
                                "app arch:" + ref.arch.toString() + " not match host architecture");
        return;
    }

    utils::Transaction t;

    taskContext.updateSubState(linglong::api::types::v1::SubState::InstallApplication,
                               "Installing application " + ref.toString());
    this->repo.pull(taskContext, ref, module);
    if (taskContext.subState() == linglong::api::types::v1::SubState::Done) {
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

    pullDependency(taskContext, *info, module);
    // check the status of pull runtime and foundation
    if (taskContext.subState() == linglong::api::types::v1::SubState::Done) {
        return;
    }

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
    auto reference = *ref;
    auto curModule = paras->package.packageManager1PackageModule.value_or("binary");

    auto refSpec =
      QString{ "%1/%2/%3" }.arg(QString::fromStdString(this->repo.getConfig().defaultRepo),
                                reference.toString(),
                                QString::fromStdString(curModule));
    auto task = std::find_if(this->taskList.cbegin(),
                             this->taskList.cend(),
                             [&refSpec](const PackageTask *task) {
                                 return refSpec == task->refSpec();
                             });
    if (task != this->taskList.cend()) {
        return toDBusReply(-1, "the target " % refSpec % " is being operated");
    }
    auto *taskPtr = new PackageTask{ connection(), refSpec };
    auto &taskRef = *(this->taskList.emplace_back(taskPtr));

    taskRef.setJob([this, &taskRef, reference, curModule]() {
        if (taskRef.subState() == linglong::api::types::v1::SubState::Done) {
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
                                  const std::string &module) noexcept
{
    LINGLONG_TRACE("uninstall ref " + ref.toString());
    taskContext.updateSubState(linglong::api::types::v1::SubState::PreAction,
                               "prepare uninstalling package");
    // TODO: 向repo请求应用运行状态
    if (taskContext.subState() == linglong::api::types::v1::SubState::Done) {
        return;
    }

    taskContext.updateSubState(linglong::api::types::v1::SubState::Uninstall, "Remove layer files");
    utils::Transaction transaction;
    transaction.addRollBack([this, &ref, &module]() noexcept {
        auto tmpTask = PackageTask::createTemporaryTask();
        this->repo.pull(tmpTask, ref, module);
    });

    auto result = this->repo.remove(ref, module);
    if (!result) {
        taskContext.updateState(linglong::api::types::v1::State::Failed,
                                LINGLONG_ERRV(result).message());
        return;
    }

    taskContext.updateState(PackageTask::State::Succeed,
                            "Uninstall " + ref.toString() + " success");
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
    auto refSpec =
      QString{ "%1/%2/%3" }.arg(QString::fromStdString(this->repo.getConfig().defaultRepo),
                                newReference.toString(),
                                QString::fromStdString(curModule));
    auto task = std::find_if(this->taskList.cbegin(),
                             this->taskList.cend(),
                             [&refSpec](const PackageTask *task) {
                                 return refSpec == task->refSpec();
                             });
    if (task != this->taskList.cend()) {
        return toDBusReply(-1, "the target " % refSpec % " is being operated");
    }
    auto *taskPtr = new PackageTask{ connection(), refSpec };
    auto &taskRef = *(this->taskList.emplace_back(taskPtr));

    taskRef.setJob([this, &taskRef, reference, newReference, curModule]() {
        if (taskRef.subState() == linglong::api::types::v1::SubState::Done) {
            return;
        }

        this->Update(taskRef, reference, newReference, curModule);
    });
    Q_EMIT TaskListChanged(taskRef.taskObjectPath());
    return utils::serialize::toQVariantMap(api::types::v1::PackageManager1PackageTaskResult{
      .taskObjectPath = taskRef.taskObjectPath().toStdString(),
      .code = 0,
      .message = (ref->toString() + " is updating").toStdString(),
    });
}

void PackageManager::Update(PackageTask &taskContext,
                            const package::Reference &ref,
                            const package::Reference &newRef) noexcept
{
    LINGLONG_TRACE("update " + ref.toString());
    taskContext.updateState(api::types::v1::State::Processing, "start to uninstalling package");

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
        if (taskContext.state() == PackageTask::State::Failed
            || taskContext.state() == PackageTask::State::Canceled) {
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

    if (taskContext.subState() == linglong::api::types::v1::SubState::Done) {
        return;
    }
    this->repo.unexportReference(ref);
    this->repo.exportReference(newRef);
    t.commit();

    taskContext.updateState(linglong::api::types::v1::State::Succeed,
                            "Upgrade " + ref.toString() + " to " + newRef.toString() + " success");

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
        auto runtimeLayerDir = repo.getLayerDir(*runtime, module);
        if (!runtimeLayerDir) {
            if (taskContext.subState() == linglong::api::types::v1::SubState::Done) {
                return;
            }

            this->repo.pull(taskContext, *runtime, module);

            if (taskContext.subState() == linglong::api::types::v1::SubState::Done) {
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
        if (taskContext.subState() == linglong::api::types::v1::SubState::Done) {
            return;
        }

        this->repo.pull(taskContext, *base, module);
        if (taskContext.subState() == linglong::api::types::v1::SubState::Done) {
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
        if (info.packageInfoV2Module != "binary") {
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

    for (auto it = target.cbegin(); it != target.cend(); ++it) {
        if (it->second != 0) {
            continue;
        }
        // NOTE: if the binary module is removed, other modules should be removed too.
        for (const auto module : { "binary", "develop" }) {
            auto layer = this->repo.getLayerDir(it->first, module);
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

            auto result = this->repo.remove(it->first, module);
            if (!result) {
                return LINGLONG_ERR(result);
            }
        }
    }
    return LINGLONG_OK;
}

void PackageManager::ReplyInteraction(QDBusObjectPath object_path, const QVariantMap &replies)
{
    Q_EMIT this->ReplyReceived(replies);
}

} // namespace linglong::service
