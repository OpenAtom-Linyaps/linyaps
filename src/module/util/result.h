/*
 * Copyright (c) 2020-2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "module/util/json.h"

namespace linglong {
namespace util {

// a stack trace result for function return
class Result
{
public:
    Result(const char *file, int line, const char *func)
        : msgMeta(MessageMeta {
            .file = file,
            .line = line,
            .func = func,
        }) {

        };

    Result(const Result &base, const char *file, int line, const char *func)
        : errorCode(base.errorCode)
        , msgMeta(MessageMeta {
              .file = file,
              .line = line,
              .func = func,
          })
        , msgMetaList(base.msgMetaList) {};

    bool success() const { return 0 == errorCode; }

    int code() const { return errorCode; }

    Result &operator<<(const QString &msg)
    {
        msgMeta.message = msg;
        msgMetaList.push_back(msgMeta);
        return *this;
    }

    Result &operator<<(int code)
    {
        errorCode = code;
        return *this;
    }

    friend QDebug operator<<(QDebug d, const linglong::util::Result &result);

private:
    struct MessageMeta {
        const char *file;
        int line;
        const char *func;
        QString message;
    };

    int errorCode = 0;
    MessageMeta msgMeta;
    QList<MessageMeta> msgMetaList {};
};

QDebug operator<<(QDebug dbg, const Result &result)
{
    for (const auto &meta : result.msgMetaList) {
        dbg << QString(meta.file) + ":" + QString("%1").arg(meta.line) + " " + QString(meta.func) << "\n";
        dbg << meta.message << "\n";
    }
    return dbg;
}

} // namespace util
} // namespace linglong

#define dResult(base) linglong::util::Result(base, __FILE__, __LINE__, __FUNCTION__)

#define dResultBase() linglong::util::Result(__FILE__, __LINE__, __FUNCTION__)
