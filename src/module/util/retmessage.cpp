/*
 * Copyright (c) 2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     liujianqiang <liujianqiang@uniontech.com>
 *
 * Maintainer: liujianqiang <liujianqiang@uniontech.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#include "retmessage.h"

void RetMessageFromUab::setState(const bool &state)
{
    this->state = state;
}

void RetMessageFromUab::setCode(const quint32 &code)
{
    this->code = code;
}

void RetMessageFromUab::setMessage(const QString &message)
{
    this->message = message;
}

bool RetMessageFromUab::getState() const
{
    return this->state;
}

quint32 RetMessageFromUab::getCode() const
{
    return this->code;
}

QString RetMessageFromUab::getMessage() const
{
    return this->message;
}
