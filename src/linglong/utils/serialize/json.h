/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_UTILS_SERIALIZE_JSON_H
#define LINGLONG_UTILS_SERIALIZE_JSON_H

#include "linglong/util/error.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSharedPointer>

#include <tuple>

namespace linglong::utils::serialize::json {
template<typename T>
std::tuple<T, util::Error> deserialize(const QByteArray &data)
{
    QJsonParseError jsonError;
    auto doc = QJsonDocument::fromJson(data, &jsonError);
    if (jsonError.error) {
        return std::make_tuple<T, util::Error>({},
                                               NewError(jsonError.error, jsonError.errorString()));
    }

    const auto v = doc.toVariant();
    if (v.canConvert<T>()) {
        return std::make_tuple<T, util::Error>(v.value<T>(), {});
    }

    return std::make_tuple<T, util::Error>({}, NewError(-1, QString("convert failed")));
}

template<typename T>
std::tuple<QByteArray, util::Error> serialize(const T &x)
{
    QVariant v = QVariant::fromValue(x);
    if (v.canConvert<QVariantList>()) {
        return std::make_tuple<QByteArray, util::Error>(
          QJsonDocument::fromVariant(v.toList()).toJson(),
          {});
    }

    if (v.canConvert<QVariantMap>()) {
        return std::make_tuple<QByteArray, util::Error>(
          QJsonDocument::fromVariant(v.toMap()).toJson(),
          {});
    }

    return std::make_tuple<QByteArray, util::Error>({}, NewError(-1, QString("convert failed")));
}

} // namespace linglong::utils::serialize::json
#endif
