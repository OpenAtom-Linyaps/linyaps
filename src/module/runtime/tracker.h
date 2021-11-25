/*
 * Copyright (c) 2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "oci.h"

class TrackerPrivate;
class Tracker : public JsonSerialize
{
    Q_OBJECT;
    Q_JSON_PROPERTY(QStringList, mounts);

public:
    explicit Tracker(QObject *parent = nullptr);
    ~Tracker() override;

    int start();

private:
    QScopedPointer<TrackerPrivate> dd_ptr;
    Q_DECLARE_PRIVATE_D(qGetPtrHelper(dd_ptr), Tracker)
};
Q_JSON_DECLARE_PTR_METATYPE(Tracker)