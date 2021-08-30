/*
 * Copyright (c) 2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
