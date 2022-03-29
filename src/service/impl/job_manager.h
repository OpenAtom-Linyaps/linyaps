/*
 * Copyright (c) 2020. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QDBusArgument>
#include <QDBusContext>
#include <QList>
#include <QObject>

#include "module/util/singleton.h"
#include "module/runtime/container.h"

class Job;
class JobManagerPrivate;
class JobManager : public QObject
    , protected QDBusContext
    , public linglong::util::Singleton<JobManager>
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.deepin.linglong.JobManager")

    friend class linglong::util::Singleton<JobManager>;

public:
    QString CreateJob(std::function<void()> f);

public Q_SLOTS:
    QStringList List() { return {}; };
    void Stop(const QString &jobID) {};

protected:
    JobManager();
    ~JobManager() override;

private:
    QScopedPointer<JobManagerPrivate> dd_ptr;
    Q_DECLARE_PRIVATE_D(qGetPtrHelper(dd_ptr), JobManager)
};