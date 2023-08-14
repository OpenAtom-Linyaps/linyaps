/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/job_manager/job.h"

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

Job::Job(std::function<void()> f)
    : func(f)
    , dd_ptr(new JobPrivate(this))
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
