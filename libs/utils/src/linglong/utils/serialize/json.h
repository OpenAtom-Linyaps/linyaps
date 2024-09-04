/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

// NOTE: DO NOT REMOVE THIS HEADER, nlohmann::json need this header to lookup function 'from_json'
#include "linglong/api/types/v1/Generators.hpp"
#include "linglong/utils/error/error.h"
#include "nlohmann/json.hpp"

#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>

namespace linglong::utils::serialize {

QJsonObject QJsonObjectfromVariantMap(const QVariantMap &vmap) noexcept;

template<typename T>
QJsonDocument toQJsonDocument(const T &x) noexcept
{
    nlohmann::json json = x;
    QJsonDocument doc;
    return QJsonDocument::fromJson(json.dump().data());
}

template<typename T>
QVariantMap toQVariantMap(const T &x) noexcept
{
    QJsonDocument doc = toQJsonDocument(x);
    Q_ASSERT(doc.isObject());
    return doc.object().toVariantMap();
}

template<typename T, typename Source>
error::Result<T> LoadJSON(const Source &content) noexcept
{
    LINGLONG_TRACE("load json");

    try {
        auto json = nlohmann::json::parse(content);
        return json.template get<T>();
    } catch (const std::exception &e) {
        return LINGLONG_ERR(content, e);
    }
}

template<typename T>
error::Result<T> LoadJSON(const nlohmann::json &content) noexcept
{
    LINGLONG_TRACE("load json");

    try {
        return content.template get<T>();
    } catch (const std::exception &e) {
        return LINGLONG_ERR(QString::fromStdString(content.dump()), e);
    }
}

template<typename T, typename Source>
error::Result<T> LoadJSON(Source &content) noexcept
{
    LINGLONG_TRACE("load json");

    try {
        auto json = nlohmann::json::parse(content);
        return json.template get<T>();
    } catch (const std::exception &e) {
        return LINGLONG_ERR(content, e);
    }
}

template<typename T>
error::Result<T> LoadJSONFile(QFile &file) noexcept
{
    LINGLONG_TRACE("load json from file" + QFileInfo(file).absoluteFilePath());

    file.open(QFile::ReadOnly);
    if (!file.isOpen()) {
        return LINGLONG_ERR("open", file);
    }

    Q_ASSERT(file.error() == QFile::NoError);

    auto content = file.readAll();
    if (file.error() != QFile::NoError) {
        return LINGLONG_ERR("read all", file);
    }

    return LoadJSON<T>(content);
}

template<typename T>
error::Result<T> LoadJSONFile(const QString &filename) noexcept
{
    QFile file = filename;
    return LoadJSONFile<T>(file);
}

template<typename T>
error::Result<T> fromQJsonDocument(const QJsonDocument &doc) noexcept
{
    auto x = LoadJSON<T>(doc.toJson().constData());

    if (!x) {
        return x;
    }
    return x;
}

template<typename T>
error::Result<T> fromQJsonObject(const QJsonObject &obj) noexcept
{
    return fromQJsonDocument<T>(QJsonDocument(obj));
}

template<typename T>
error::Result<T> fromQJsonValue(const QJsonValue &v) noexcept
{
    if (v.isArray()) {
        return fromQJsonDocument<T>(QJsonDocument(v.toArray()));
    }
    if (v.isObject()) {
        return fromQJsonObject<T>(v.toObject());
    }

    Q_ASSERT("QJsonValue should be object or array.");
    abort();
}

template<typename T>
error::Result<T> fromQVariantMap(const QVariantMap &vmap)
{
    return fromQJsonObject<T>(QJsonObjectfromVariantMap(vmap));
}

} // namespace linglong::utils::serialize
