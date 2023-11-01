/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_PACKAGE_MANAGER_IMPL_JOB_MANAGER_H_
#define LINGLONG_SRC_PACKAGE_MANAGER_IMPL_JOB_MANAGER_H_

#include "linglong/repo/ostree_repo.h"
#include "linglong/runtime/container.h"
#include "linglong/util/singleton.h"

#include <QDBusArgument>
#include <QDBusContext>
#include <QList>
#include <QObject>

namespace linglong::job_manager {

class Job;
class JobManagerPrivate;

class JobManager : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.deepin.linglong.JobManager")

public:
    QString CreateJob(std::function<void()> f);
    JobManager(linglong::repo::OSTreeRepo &ostreeRepo, QObject *parent = nullptr);

public Q_SLOTS:
    QStringList List();
    void Start(const QString &jobId);
    void Stop(const QString &jobId);
    void Cancel(const QString &jobId);

private:
    linglong::repo::OSTreeRepo &ostreeRepo;
};

} // namespace linglong::job_manager
#endif
