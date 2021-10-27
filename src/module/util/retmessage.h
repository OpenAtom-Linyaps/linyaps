/*
 * Copyright (c) 2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     liujianqiang <liujianqiang@uniontech.com>
 *
 * Maintainer: liujianqiang <liujianqiang@uniontech.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once
#include <QString>

class RetMessageFromUab
{
private:
    bool state;
    quint32 code;
    QString message;

public:
    RetMessageFromUab() = default;
    void setState(const bool &state);
    void setCode(const quint32 &code);
    void setMessage(const QString &message);
    bool getState() const;
    quint32 getCode() const;
    QString getMessage() const;
};
