/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "linglong/repo/ostree_repo.h"
#include "package_task.h"
#include "task.h"

#include <QDBusArgument>
#include <QDBusContext>
#include <QList>
#include <QObject>

namespace linglong::service {

class PackageManager : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.deepin.linglong.PackageManager")
    Q_PROPERTY(QVariantMap Configuration READ getConfiguration WRITE setConfiguration)

public:
    PackageManager(linglong::repo::OSTreeRepo &repo, QObject *parent);

    ~PackageManager() override = default;
    PackageManager(const PackageManager &) = delete;
    PackageManager(PackageManager &&) = delete;
    auto operator=(const PackageManager &) -> PackageManager & = delete;
    auto operator=(PackageManager &&) -> PackageManager & = delete;
    void Update(PackageTask &taskContext,
                const package::Reference &ref,
                const package::Reference &newRef,
                bool develop) noexcept;

public
    Q_SLOT : auto getConfiguration() const noexcept -> QVariantMap;
    auto setConfiguration(const QVariantMap &parameters) noexcept -> QVariantMap;
    auto Install(const QVariantMap &parameters) noexcept -> QVariantMap;
    void installRef(PackageTask &taskContext, const package::Reference &ref, bool devel) noexcept;
    auto InstallFromFile(const QDBusUnixFileDescriptor &fd,
                         const QString &fileType) noexcept -> QVariantMap;
    auto Uninstall(const QVariantMap &parameters) noexcept -> QVariantMap;
    void Uninstall(PackageTask &taskContext, const package::Reference &ref, bool devel) noexcept;
    auto Update(const QVariantMap &parameters) noexcept -> QVariantMap;
    auto Search(const QVariantMap &parameters) noexcept -> QVariantMap;
    auto Prune() noexcept -> QVariantMap;
    auto SetRunningState(const QVariantMap &parameters) noexcept -> QVariantMap;
    void replyInteraction(const QString &interactionID, const QVariantMap &replies);

Q_SIGNALS:
    void TaskListChanged(QString taskID);
    void TaskChanged(QString taskObjectPath, QString message);

private:
    QVariantMap installFromLayer(const QDBusUnixFileDescriptor &fd) noexcept;
    QVariantMap installFromUAB(const QDBusUnixFileDescriptor &fd) noexcept;
    static utils::error::Result<api::types::v1::MinifiedInfo>
    updateMinifiedInfo(const QFileInfo &file, const QString &appRef, const QString &uuid) noexcept;
    void pullDependency(PackageTask &taskContext,
                        const api::types::v1::PackageInfoV2 &info,
                        bool develop) noexcept;
    linglong::repo::OSTreeRepo &repo; // NOLINT
    std::list<PackageTask> taskList;
    // 正在运行的任务ID
    QString runningTaskID;
};

} // namespace linglong::service
