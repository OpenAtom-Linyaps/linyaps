/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "error.h"

#include <QJsonDocument>
#include <QJsonObject>

#include <iostream>

namespace linglong {
namespace util {

class ErrorPrivate
{
public:
    ErrorPrivate() = default;

    ErrorPrivate(const char *file,
                 int line,
                 const char *func,
                 const QString &message,
                 const Error &reason = Success())
        : context(new QMessageLogContext(file, line, func, "linglong"))
        , message(message)
        , reason(reason)
    {
    }

    const QSharedPointer<const QMessageLogContext> context;
    const QString message = "";
    const QSharedPointer<const ErrorPrivate> reason;
};

Error::Error(const char *file, int line, const char *func, int code, const QString &msg)
    : QSharedPointer<ErrorPrivate>(new ErrorPrivate(file, line, func, msg))
    , m_code(code)
{
}

Error::Error(const char *file, int line, const char *func, const Error &reason, const QString &msg)
    : QSharedPointer<ErrorPrivate>(new ErrorPrivate(file, line, func, msg, reason))
    , m_code(reason.code())
{
}

Error::operator bool() const
{
    return this->data() != nullptr && this->m_code != 0;
}

int Error::code() const
{
    return this->m_code;
}

QString Error::message() const
{
    return this->data()->message;
}

QString Error::toJson() const
{
    QJsonObject obj;
    obj["code"] = this->m_code;
    obj["message"] = this->data()->message;
    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}

QDebug operator<<(QDebug debug, const Error &error)
{
    bool first = true;
    const ErrorPrivate *err = error.data();
    if (err == nullptr) {
        debug << "success";
    }
    while (err != nullptr) {
        debug.noquote() << QString("%1 occurs in function")
                             .arg(first ? QString("Error (code=%1)").arg(error.m_code)
                                        : "\nCaused by error")
                        << Qt::endl
                        << (err->context->function ? err->context->function
                                                   : "MISSING function name")
                        << Qt::endl
                        << QString("at %1:%2")
                             .arg(err->context->file ? err->context->file : "MISSING file name")
                             .arg(err->context->line)
                        << Qt::endl
                        << "message:" << err->message;
        err = err->reason.data();
        first = false;
    }
    return debug;
}

/*!
 * Support gtest print pretty assert fail output
 * @param obj
 * @param os
 */
void PrintTo(const linglong::util::Error &obj, ::std::ostream *os)
{
    bool first = true;
    const linglong::util::ErrorPrivate *err = obj.data();
    if (err == nullptr) {
        *os << std::string("success");
    }
    while (err != nullptr) {
        *os << QString("%1 occurs in function")
                 .arg(first ? QString("Error (code=%1)").arg(obj.code()) : "\nCaused by error")
                 .toStdString()
            << std::endl
            << (err->context->function ? err->context->function : "MISSING function name")
            << std::endl
            << QString(" at %1:%2")
                 .arg(err->context->file ? err->context->file : "MISSING file name")
                 .arg(err->context->line)
                 .toStdString()
            << std::endl
            << "message: " << err->message.toStdString();
        err = err->reason.data();
        first = false;
    }
}

} // namespace util
} // namespace linglong
