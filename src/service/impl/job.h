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

#include <QScopedPointer>
#include <QThread>

class JobPrivate;
class Job : public QThread
{
    Q_OBJECT
public:
    Job(std::function<void()> f, QObject *parent);
    ~Job() override;

public Q_SLOTS:
    QString Status() const;
    int Progress() const;
    void run();

Q_SIGNALS: // SIGNALS
    void Finish();

public:
    std::function<void()> func;

private:
    QScopedPointer<JobPrivate> dd_ptr;
    Q_DECLARE_PRIVATE_D(qGetPtrHelper(dd_ptr), Job)
};
