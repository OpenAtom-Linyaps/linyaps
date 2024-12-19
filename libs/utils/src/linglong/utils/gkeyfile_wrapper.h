/*
 * SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "linglong/utils/error/error.h"

#include <glib.h>
#include <tl/expected.hpp>

#include <QFile>
#include <QString>

namespace linglong::utils {

class GKeyFileWrapper final
{
public:
    using GroupName = QString;
    static constexpr auto DesktopEntry{ "Desktop Entry" };
    static constexpr auto DBusService{ "D-BUS Service" };
    static constexpr auto SystemdService{ "Service" };
    static constexpr auto ContextMenu{ "Menu Entry" };

    GKeyFileWrapper(GKeyFileWrapper &&) = default;
    GKeyFileWrapper(const GKeyFileWrapper &) = delete;
    auto operator=(GKeyFileWrapper &&) -> GKeyFileWrapper & = default;
    auto operator=(const GKeyFileWrapper &) -> GKeyFileWrapper & = delete;
    ~GKeyFileWrapper() = default;

    static auto New(const QString &filePath) -> error::Result<GKeyFileWrapper>
    {
        LINGLONG_TRACE(QString("create GKeyFileWrapper for %1").arg(filePath));

        auto desktopEntryFile = QFile(filePath);
        if (!desktopEntryFile.exists()) {
            return LINGLONG_ERR("no such file");
        }

        g_autoptr(GError) gErr = nullptr;
        GKeyFileWrapper entry;
        g_key_file_load_from_file(entry.gKeyFile.get(),
                                  filePath.toLocal8Bit().constData(),
                                  G_KEY_FILE_KEEP_TRANSLATIONS,
                                  &gErr);
        if (gErr != nullptr) {
            return LINGLONG_ERR("g_key_file_load_from_file", gErr);
        }

        return entry;
    }

    void setValue(const QString &key, const QString &value, const GroupName &group)
    {
        g_key_file_set_string(this->gKeyFile.get(),
                              group.toLocal8Bit().constData(),
                              key.toLocal8Bit().constData(),
                              value.toLocal8Bit().constData());
    }

    template<typename Value>
    auto getValue(const QString &key, const GroupName &group) const -> error::Result<Value>
    {
        LINGLONG_TRACE(QString("get %1 from %2").arg(key, group));

        g_autoptr(GError) gErr = nullptr;
        g_autofree gchar *value = g_key_file_get_string(this->gKeyFile.get(),
                                                        group.toLocal8Bit().constData(),
                                                        key.toLocal8Bit().constData(),
                                                        &gErr);

        if (gErr != nullptr) {
            return LINGLONG_ERR("g_key_file_get_string", gErr);
        }

        return value;
    }

    auto getGroups() -> QStringList
    {
        gsize length = 0;
        g_auto(GStrv) groups = g_key_file_get_groups(this->gKeyFile.get(), &length);

        QStringList result;
        for (gsize i = 0; i < length; i++) {
            auto group = groups[i]; // NOLINT
            result << group;
        }

        return result;
    }

    auto getkeys(const QString &group) -> error::Result<QStringList>
    {
        LINGLONG_TRACE("get keys from " + group);

        g_autoptr(GError) gErr = nullptr;
        gsize length{ 0 };
        g_auto(GStrv) keys = g_key_file_get_keys(this->gKeyFile.get(),
                                                 group.toLocal8Bit().constData(),
                                                 &length,
                                                 &gErr);
        if (gErr != nullptr) {
            return LINGLONG_ERR("g_key_file_get_keys", gErr);
        }

        QStringList result;
        for (gsize i = 0; i < length; i++) {
            auto key = keys[i]; // NOLINT
            result << key;
        }

        return result;
    }

    auto saveToFile(const QString &filepath) -> error::Result<void>
    {
        LINGLONG_TRACE(QString("save to %1").arg(filepath));

        g_autoptr(GError) gErr = nullptr;

        g_key_file_save_to_file(this->gKeyFile.get(), filepath.toLocal8Bit().constData(), &gErr);
        if (gErr != nullptr) {
            return LINGLONG_ERR("g_key_file_save_to_file", gErr);
        }

        return LINGLONG_OK;
    }

    auto hasKey(const QString &key, const GroupName &group) -> error::Result<bool>
    {
        LINGLONG_TRACE(QString("check %1 is in %2 or not").arg(key).arg(group));

        g_autoptr(GError) gErr = nullptr;
        if (g_key_file_has_key(this->gKeyFile.get(),
                               group.toLocal8Bit().constData(),
                               key.toLocal8Bit().constData(),
                               &gErr)
            == FALSE) {
            if (gErr != nullptr) {
                return LINGLONG_ERR("g_key_file_has_key", gErr);
            }
            return false;
        }
        return true;
    }

private:
    GKeyFileWrapper()
        : gKeyFile(std::unique_ptr<GKeyFile, decltype(&g_key_file_free)>(g_key_file_new(),
                                                                         g_key_file_free)) {};
    std::unique_ptr<GKeyFile, decltype(&g_key_file_free)> gKeyFile;
};

} // namespace linglong::utils
