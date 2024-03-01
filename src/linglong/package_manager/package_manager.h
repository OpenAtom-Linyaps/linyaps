/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_PACKAGE_MANAGER_PACKAGE_MANAGER_H_
#define LINGLONG_SRC_PACKAGE_MANAGER_PACKAGE_MANAGER_H_

#include "linglong/package_manager/task.h"
#include "linglong/repo/ostree_repo.h"

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
    virtual void Install(const std::shared_ptr<InstallTask> &taskContext,
                         const package::Reference &ref,
                         bool devel) noexcept;
    virtual void Update(const std::shared_ptr<InstallTask> &taskContext,
                        const package::Reference &ref,
                        const package::Reference &newRef,
                        bool devel) noexcept;

public Q_SLOTS: // NOLINT
    virtual auto getConfiguration() const noexcept -> QVariantMap;
    virtual auto setConfiguration(const QVariantMap &parameters) noexcept -> QVariantMap;
    virtual auto Install(const QVariantMap &parameters) noexcept -> QVariantMap;
    virtual auto InstallLayer(const QDBusUnixFileDescriptor &fd) noexcept -> QVariantMap;
    virtual auto Uninstall(const QVariantMap &parameters) noexcept -> QVariantMap;
    virtual auto Update(const QVariantMap &parameters) noexcept -> QVariantMap;
    virtual auto Search(const QVariantMap &parameters) noexcept -> QVariantMap;
    virtual void CancelTask(const QString &taskID) noexcept;

Q_SIGNALS:
    void TaskChanged(QString taskID, QString percentage, QString message, int status);

private:
    linglong::repo::OSTreeRepo &repo; // NOLINT
    std::map<QString, std::shared_ptr<InstallTask>> taskMap;
};

} // namespace linglong::service

#endif
