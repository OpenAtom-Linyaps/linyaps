// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QWidget>

class CacheDialog : public QWidget
{
    Q_OBJECT
public:
    explicit CacheDialog(const QString &id);
    ~CacheDialog() override = default;

private:
    QString id;
    int dotCount = 0;
};
