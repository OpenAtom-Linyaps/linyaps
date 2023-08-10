/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/util/qserializer/json.h"

namespace linglong {
namespace util {

std::tuple<QVariantMap, Error> mapFromJSON(const QString &filePath)
{
    QFile file(filePath);
    file.open(QIODevice::ReadOnly);
    auto bytes = file.readAll();
    return mapFromJSON(bytes);
}

std::tuple<QVariantMap, Error> mapFromJSON(const QByteArray &bytes)
{
    QJsonParseError jsonErr;
    auto doc = QJsonDocument::fromJson(bytes, &jsonErr);
    if (jsonErr.error) {
        return std::make_tuple<QVariantMap, Error>({},
                                                   NewError(jsonErr.error, jsonErr.errorString()));
    }
    if (!doc.isObject()) {
        return std::make_tuple<QVariantMap, Error>(
                {},
                NewError(-1,
                         QString("Failed to create QVariantMap from JSON: JSON document is not an "
                                 "object")));
    }

    return { doc.object().toVariantMap(), {} };
}

std::tuple<QVariantList, Error> listFromJSON(const QByteArray &bytes)
{
    QJsonParseError jsonErr;
    auto doc = QJsonDocument::fromJson(bytes, &jsonErr);
    if (jsonErr.error) {
        return std::make_tuple<QVariantList, Error>({},
                                                    NewError(jsonErr.error, jsonErr.errorString()));
    }
    if (!doc.isArray()) {
        return std::make_tuple<QVariantList, Error>(
                {},
                NewError(-1,
                         QString("Failed to create QVariantList from JSON: JSON document is not an "
                                 "object")));
    }

    return { doc.array().toVariantList(), {} };
}

} // namespace util
} // namespace linglong
