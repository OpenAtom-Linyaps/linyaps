/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_PACKAGE_MANAGER_PACKAGE_MANAGER_H_
#define LINGLONG_SRC_PACKAGE_MANAGER_PACKAGE_MANAGER_H_

#include "linglong/repo/ostree_repo.h"
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
    void Install(InstallTask &taskContext, const package::Reference &ref, bool devel) noexcept;
    void Update(InstallTask &taskContext,
                const package::Reference &ref,
                const package::Reference &newRef,
                bool develop) noexcept;

public
    Q_SLOT : auto getConfiguration() const noexcept -> QVariantMap;
    auto setConfiguration(const QVariantMap &parameters) noexcept -> QVariantMap;
    auto Install(const QVariantMap &parameters) noexcept -> QVariantMap;
    auto InstallFromFile(const QDBusUnixFileDescriptor &fd,
                         const QString &fileType) noexcept -> QVariantMap;
    auto Uninstall(const QVariantMap &parameters) noexcept -> QVariantMap;
    auto Update(const QVariantMap &parameters) noexcept -> QVariantMap;
    auto Search(const QVariantMap &parameters) noexcept -> QVariantMap;
    void CancelTask(const QString &taskID) noexcept;

Q_SIGNALS:
    void TaskChanged(QString taskID, QString percentage, QString message, int status);

private:
    QVariantMap installFromLayer(const QDBusUnixFileDescriptor &fd) noexcept;
    QVariantMap installFromUAB(const QDBusUnixFileDescriptor &fd) noexcept;
    static utils::error::Result<api::types::v1::MinifiedInfo>
    updateMinifiedInfo(const QFileInfo &file, const QString &appRef, const QString &uuid) noexcept;
    void pullDependency(InstallTask &taskContext,
                        const api::types::v1::PackageInfoV2 &info,
                        bool develop) noexcept;
    linglong::repo::OSTreeRepo &repo; // NOLINT
    std::list<InstallTask> taskList;
};

} // namespace linglong::service

#endif
