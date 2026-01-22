/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "linglong/common/formatter.h"
#include "linglong/utils/error/error.h"
#include "linglong/utils/serialize/json.h"

#include <gio/gio.h>
#include <glib.h>

#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QVariantMap>

namespace linglong::common::serialize {

QJsonObject QJsonObjectfromVariantMap(const QVariantMap &vmap) noexcept;

template <typename T>
QJsonDocument toQJsonDocument(const T &x) noexcept
{
    nlohmann::json json = x;
    return QJsonDocument::fromJson(json.dump().data());
}

template <typename T>
QVariantMap toQVariantMap(const T &x) noexcept
{
    QJsonDocument doc = toQJsonDocument(x);
    Q_ASSERT(doc.isObject());
    return doc.object().toVariantMap();
}

template <typename T>
utils::error::Result<T> fromQJsonDocument(const QJsonDocument &doc) noexcept
{
    return utils::serialize::LoadJSON<T>(doc.toJson(QJsonDocument::Compact).toStdString());
}

template <typename T>
utils::error::Result<T> fromQJsonObject(const QJsonObject &obj) noexcept
{
    return fromQJsonDocument<T>(QJsonDocument(obj));
}

template <typename T>
utils::error::Result<T> fromQVariantMap(const QVariantMap &vmap)
{
    return fromQJsonObject<T>(QJsonObjectfromVariantMap(vmap));
}

template <typename T>
utils::error::Result<T> LoadJSONFile(GFile *file) noexcept
{
    g_autofree char *path = g_file_get_path(file);
    LINGLONG_TRACE(fmt::format("load json from {}", path));

    g_autoptr(GError) gErr = nullptr;
    g_autofree gchar *content = nullptr;
    gsize length;

    if (!g_file_load_contents(file, nullptr, &content, &length, nullptr, &gErr)) {
        return LINGLONG_ERR(fmt::format("g_file_load_contents {}", *gErr));
    }

    return utils::serialize::LoadJSON<T>(content);
}

} // namespace linglong::common::serialize
