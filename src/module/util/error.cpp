/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "error.h"

namespace linglong {
namespace util {

Error::Error(const char *file,
             int line,
             const char *func,
             const Error &base,
             int code,
             const QString &msg)
    : errorCode(code)
    , msgMeta(MessageMeta{
              .file = file,
              .line = line,
              .func = func,
              .message = msg,
      })
    , msgMetaList(base.msgMetaList)
{
    msgMetaList.push_back(msgMeta);
}

Error::Error(const char *file, int line, const char *func, int code, const QString &msg)
    : errorCode(code)
    , msgMeta(MessageMeta{
              .file = file,
              .line = line,
              .func = func,
              .message = msg,
      })
{
    msgMetaList.push_back(msgMeta);
}

Error::Error(const char *file, int line, const char *func, const QString &msg, const Error &base)
    : errorCode(base.code())
    , msgMeta(MessageMeta{
              .file = file,
              .line = line,
              .func = func,
              .message = msg,
      })
    , msgMetaList(base.msgMetaList)
{
    msgMetaList.push_back(msgMeta);
};

bool Error::success() const
{
    return 0 == errorCode;
}

int Error::code() const
{
    return errorCode;
}

QString Error::message() const
{
    return msgMeta.message;
}

Error &Error::operator<<(const QString &msg)
{
    msgMeta.message = msg;
    msgMetaList.push_back(msgMeta);
    return *this;
}

Error &Error::operator<<(int code)
{
    errorCode = code;
    return *this;
}

QString Error::toJson() const
{
    QJsonObject obj;
    obj["code"] = errorCode;
    obj["message"] = msgMeta.message;
    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}

QDebug operator<<(QDebug dbg, const Error &result)
{
    dbg << "\n";
    for (const auto &meta : result.msgMetaList) {
        dbg << QString(meta.file) + ":" + QString("%1").arg(meta.line) + ":" + QString(meta.func)
            << "\n";
        dbg << meta.message << "\n";
    }
    return dbg;
}

} // namespace util
} // namespace linglong
