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

#include <QtConcurrent/QtConcurrentRun>
#include <QDBusConnection>

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

Job::Job(const std::function<void(Job *)> &f, QObject *parent)
    : QObject(parent)
    , dd_ptr(new JobPrivate(this))
{
    QtConcurrent::run([=]() {
        f(this);
        Q_EMIT this->Finish();
        this->deleteLater();
    });
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

Job::~Job() = default;
