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

#include <tuple>

namespace linglong {
namespace util {

/*!
 * Error is a stack trace error handler when function return
 */
class Error
{
public:
    Error(const Error &err){
        errorCode = err.errorCode;
        msgMeta = err.msgMeta;
        msgMetaList = err.msgMetaList;
    }

    Error(const char *file, int line, const char *func, const Error &base, int code = 0, const QString &msg = "")
        : errorCode(code)
        , msgMeta(MessageMeta {
              .file = file,
              .line = line,
              .func = func,
              .message = msg,
          })
        , msgMetaList(base.msgMetaList)
    {
        msgMetaList.push_back(msgMeta);
    };

    Error(const char *file, int line, const char *func, int code = 0, const QString &msg = "")
        : errorCode(code)
        , msgMeta(MessageMeta {
              .file = file,
              .line = line,
              .func = func,
              .message = msg,
          })
    {
        msgMetaList.push_back(msgMeta);
    };

    Error(const char *file, int line, const char *func, const QString &msg, const Error &base)
        : errorCode(base.code())
        , msgMeta(MessageMeta {
              .file = file,
              .line = line,
              .func = func,
              .message = msg,
          })
        , msgMetaList(base.msgMetaList)
    {
        msgMetaList.push_back(msgMeta);
    };

    bool success() const { return 0 == errorCode; }

    int code() const { return errorCode; }

    Error &operator<<(const QString &msg)
    {
        msgMeta.message = msg;
        msgMetaList.push_back(msgMeta);
        return *this;
    }

    Error &operator<<(int code)
    {
        errorCode = code;
        return *this;
    }

    QString toJson() const
    {
        QJsonObject obj;
        obj["code"] = errorCode;
        obj["message"] = msgMeta.message;
        return QJsonDocument(obj).toJson(QJsonDocument::Compact);
    }

    friend QDebug operator<<(QDebug d, const linglong::util::Error &result);

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

inline QDebug operator<<(QDebug dbg, const Error &result)
{
    dbg << "\n";
    for (const auto &meta : result.msgMetaList) {
        dbg << QString(meta.file) + ":" + QString("%1").arg(meta.line) + ":" + QString(meta.func) << "\n";
        dbg << meta.message << "\n";
    }
    return dbg;
}

//
// template<typename T>
// class Result
//{
//    typedef T Type;
//
// public:
//    inline explicit Result(const Error &error)
//        : m_error(error)
//        , m_data(Type())
//    {
//    }
//
// private:
//    Error m_error;
//    Type m_data;
//};

} // namespace util
} // namespace linglong

#define NewError(...) linglong::util::Error(__FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)

#define NoError() linglong::util::Error(__FILE__, __LINE__, __FUNCTION__)

#define WrapError(base, ...) linglong::util::Error(__FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__, base)
