/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_MODULE_UTIL_QSERIALIZER_JSON_H
#define LINGLONG_SRC_MODULE_UTIL_QSERIALIZER_JSON_H

#include "module/util/error.h"
#include "module/util/qserializer/qserializer.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVariantList>
#include <QVariantMap>

#include <type_traits>

namespace linglong {
namespace util {

std::tuple<QVariantMap, Error> mapFromJSON(const QByteArray &bytes);
std::tuple<QVariantList, Error> listFromJSON(const QByteArray &bytes);



template<typename T>
typename std::enable_if<!isQList<T>::value, std::tuple<T, Error>>::type
fromJSON(const QByteArray &bytes)
{
    auto [result, err] = mapFromJSON(bytes);
    if (err) {
        return std::make_tuple<T, Error>({}, WrapError(err, ""));
    }
    QVariant v = result;
    if (!v.canConvert<T>()) {
        return std::make_tuple<T, Error>(
                {},
                NewError(-1,
                         QString("Failed to convert QVariantMap to %1.")
                                 .arg(QString(QMetaType::fromType<T>().name()))));
    }
    return { v.value<T>(), {} };
}

template<typename T>
typename std::enable_if<isQList<T>::value, std::tuple<T, Error>>::type
fromJSON(const QByteArray &bytes)
{
    auto [result, err] = listFromJSON(bytes);
    if (err) {
        return std::make_tuple<T, Error>({}, WrapError(err, ""));
    }
    QVariant v = result;
    if (!v.canConvert<T>()) {
        return std::make_tuple<T, Error>(
                {},
                NewError(-1,
                         QString("Failed to convert QVariantMap to %1.")
                                 .arg(QString(QMetaType::fromType<T>().name()))));
    }
    return std::make_tuple<T, Error>(v.value<T>(), {});
}

template<typename T>
static auto fromJSON(const QString &filePath) -> std::tuple<T, Error>
{
    QFile file(filePath);
    file.open(QIODevice::ReadOnly);
    auto bytes = file.readAll();
    return fromJSON<T>(bytes);
}

template<typename T>
std::tuple<QByteArray, Error> toJSON(const T &x)
{
    auto v = QVariant::fromValue<T>(x);
    qDebug() << v;
    if (!v.template canConvert<QVariantMap>()) {
        return std::make_tuple<QByteArray, Error>(
                {},
                NewError(-1,
                         QString("Failed to convert %1 to QVariantMap.")
                                 .arg(QString(QMetaType::fromType<T>().name()))));
    }

    v = v.toMap();

    //    if (!v.template canConvert<QJsonObject>()) {
    //        return std::make_tuple<QByteArray, Error>(
    //                {},
    //                NewError(-1, "Failed to convert QVariantMap to QJsonObject."));
    //    }

    return { QJsonDocument(v.template value<QJsonObject>()).toJson(), {} };
}

template<typename T>
std::tuple<QByteArray, Error> toJSON(const QList<T> &x)
{
    QVariant v = x;
    if (!v.canConvert<QVariantList>()) {
        return { {},
                 NewError(-1,
                          QString("Failed to convert %1 to QVariantList.")
                                  .arg(QString(QMetaType::fromType<T>().name()))) };
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
