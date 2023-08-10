/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_PACKAGE_MANAGER_IMPL_JOB_MANAGER_H_
#define LINGLONG_SRC_PACKAGE_MANAGER_IMPL_JOB_MANAGER_H_

#include "linglong/runtime/container.h"
#include "linglong/util/singleton.h"

#include <QDBusArgument>
#include <QDBusContext>
#include <QList>
#include <QObject>

class Job;
class JobManagerPrivate;

class JobManager : public QObject,
                   protected QDBusContext,
                   public linglong::util::Singleton<JobManager>
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.deepin.linglong.JobManager")

    friend class linglong::util::Singleton<JobManager>;

public:
    QString CreateJob(std::function<void()> f);

public Q_SLOTS:
    QStringList List();
    void Start(const QString &jobId);
    void Stop(const QString &jobId);
    void Cancel(const QString &jobId);

protected:
    JobManager();
    ~JobManager() override;

private:
    QScopedPointer<JobManagerPrivate> dd_ptr;
    Q_DECLARE_PRIVATE_D(qGetPtrHelper(dd_ptr), JobManager)
};
#endif
