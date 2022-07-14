/*
 * Copyright (c) 2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "job.h"

// #include <QtConcurrent/QtConcurrentRun>
// #include <QDBusConnection>

class JobPrivate
{
public:
    explicit JobPrivate(Job *parent)
        : q_ptr(parent)
    {
    }

    Job *q_ptr = nullptr;

    int progress;
};

Job::Job(std::function<void()> f, QObject *parent)
    :func(f)
    ,dd_ptr(new JobPrivate(this))
{
}

int Job::Progress() const
{
    Q_D(const Job);
    return d->progress;
}

QString Job::Status() const
{
    return QString();
}

void Job::run()
{
    func();
    Q_EMIT this->Finish();
    this->deleteLater();
}

Job::~Job() = default;
