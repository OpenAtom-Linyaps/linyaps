/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_PACKAGE_MANAGER_IMPL_JOB_H_
#define LINGLONG_SRC_PACKAGE_MANAGER_IMPL_JOB_H_

#include <QScopedPointer>
#include <QThread>

namespace linglong::job_manager {

class JobPrivate;

class Job : public QThread
{
    Q_OBJECT
public:
    Job(std::function<void()> f);
    ~Job() override;

public Q_SLOTS:
    QString Status() const;
    int Progress() const;
    void run() override;

Q_SIGNALS: // SIGNALS
    void Finish();

public:
    std::function<void()> func;

private:
    QScopedPointer<JobPrivate> dd_ptr;
    Q_DECLARE_PRIVATE_D(qGetPtrHelper(dd_ptr), Job)
};

} // namespace linglong::job_manager
#endif
