/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_MODULE_UTIL_QSERIALIZER_JSON_H
#define LINGLONG_SRC_MODULE_UTIL_QSERIALIZER_JSON_H

#include "module/util/error.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVariantList>
#include <QVariantMap>

namespace linglong {
namespace util {

std::tuple<QVariantMap, Error> mapFromJSON(const QByteArray &bytes);
std::tuple<QVariantList, Error> listFromJSON(const QByteArray &bytes);

template<typename T>
std::tuple<T, Error> fromJSON(const QString &filePath)
{
    QFile file(filePath);
    file.open(QIODevice::ReadOnly);
    auto bytes = file.readAll();
    return fromJSON<T>(bytes);
}

template<typename T>
std::tuple<T, Error> fromJSON(const QByteArray &bytes)
{
    auto result = mapFromJSON(bytes);
    auto err = std::get<1>(result);
    if (!err.success()) {
        return { {}, WrapError(err, "") };
    }
    QVariant v = std::get<0>(result);
    if (!v.canConvert<T>()) {
        return { {},
                 NewError(-1,
                          QString("Failed to convert QVariantMap to %1.")
                                  .arg(QMetaType::fromType<T>().name())) };
    }
    return { v.value<T>(), {} };
}

template<typename T>
std::tuple<QList<T>, Error> fromJSON(const QByteArray &bytes)
{
    auto result = listFromJSON(bytes);
    auto err = std::get<1>(result);
    if (!err.success()) {
        return { {}, WrapError(err, "") };
    }
    QVariant v = std::get<0>(result);
    if (!v.canConvert<T>()) {
        return { {},
                 NewError(-1,
                          QString("Failed to convert QVariantMap to %1.")
                                  .arg(QMetaType::fromType<T>().name())) };
    }
    return { v.value<T>(), {} };
}

template<typename T>
std::tuple<QByteArray, Error> toJSON(const T &x)
{
    QVariant v = x;
    if (!v.canConvert<QVariantMap>()) {
        return { {},
                 NewError(-1,
                          QString("Failed to convert %1 to QVariantMap.")
                                  .arg(QMetaType::fromType<T>().name())) };
    }

    v = v.toMap();
    if (!v.canConvert<QJsonObject>()) {
        return { {}, NewError(-1, "Failed to convert QVariantMap to QJsonObject.") };
    }

    return { QJsonDocument(v.value<QJsonObject>()).toJson(), {} };
}

template<typename T>
std::tuple<QByteArray, Error> toJSON(const QList<T> &x)
{
    QVariant v = x;
    if (!v.canConvert<QVariantList>()) {
        return { {},
                 NewError(-1,
                          QString("Failed to convert %1 to QVariantList.")
                                  .arg(QMetaType::fromType<T>().name())) };
    }

    v = v.toList();
    if (!v.canConvert<QJsonArray>()) {
        return { {}, NewError(-1, "Failed to convert QVariantList to QJsonArray.") };
    }

    return { QJsonDocument(v.value<QJsonArray>()).toJson(), {} };
}

} // namespace util
} // namespace linglong

#endif
