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

#include <QObject>
#include <QScopedPointer>
#include <QDBusContext>

class JobPrivate;
class Job : public QObject
    , protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.deepin.linglong.Job")
public:
    Job(const std::function<void(Job *jr)> &f, QObject *parent);
    ~Job() override;

public Q_SLOTS:
    QString Status() const;
    int Progress() const;

Q_SIGNALS: // SIGNALS
    void Finish();

private:
    QScopedPointer<JobPrivate> dd_ptr;
    Q_DECLARE_PRIVATE_D(qGetPtrHelper(dd_ptr), Job)
};
