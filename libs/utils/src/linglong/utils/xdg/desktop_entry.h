/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "linglong/utils/error/error.h"

#include <glib.h>
#include <tl/expected.hpp>

#include <QFile>
#include <QString>

namespace linglong::utils::xdg {

class DesktopEntry final
{
    // constructors
public:
    DesktopEntry(DesktopEntry &&) = default;
    DesktopEntry(const DesktopEntry &) = delete;
    auto operator=(DesktopEntry &&) -> DesktopEntry & = default;
    auto operator=(const DesktopEntry &) -> DesktopEntry & = delete;
    ~DesktopEntry() = default;

    static auto New(const QString &filePath) -> error::Result<DesktopEntry>
    {
        LINGLONG_TRACE(QString("create DesktopEntry for %1").arg(filePath));

        auto desktopEntryFile = QFile(filePath);
        if (!desktopEntryFile.exists()) {
            return LINGLONG_ERR("no such desktop entry file");
        }

        g_autoptr(GError) gErr = nullptr;
        DesktopEntry entry;
        g_key_file_load_from_file(entry.gKeyFile.get(),
                                  filePath.toLocal8Bit().constData(),
                                  G_KEY_FILE_KEEP_TRANSLATIONS,
                                  &gErr);
        if (gErr != nullptr) {
            return LINGLONG_ERR("g_key_file_load_from_file", gErr);
        }

        return entry;
    }

private:
    DesktopEntry()
        : gKeyFile(std::unique_ptr<GKeyFile, decltype(&g_key_file_free)>(g_key_file_new(),
                                                                         g_key_file_free)) { };

    // types
public:
    using SectionName = QString;

public:
    static const SectionName MainSection;

    // data members
private:
    std::unique_ptr<GKeyFile, decltype(&g_key_file_free)> gKeyFile;

    // methods
public:
    template <typename Value>
    void setValue(const QString &key, const Value &value, const SectionName &section = MainSection);

    template <typename Value>
    auto getValue(const QString &key, const SectionName &section = MainSection) const
      -> error::Result<Value>;

    auto groups() -> QStringList
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
};

template <>
inline void DesktopEntry::setValue(const QString &key,
                                   const QString &value,
                                   const SectionName &section)
{
    g_key_file_set_string(this->gKeyFile.get(),
                          section.toLocal8Bit().constData(),
                          key.toLocal8Bit().constData(),
                          value.toLocal8Bit().constData());
}

template <>
[[nodiscard]] inline auto DesktopEntry::getValue(const QString &key,
                                                 const SectionName &section) const
  -> error::Result<QString>
{
    LINGLONG_TRACE(QString("get %1 from %2").arg(key, section));

    g_autoptr(GError) gErr = nullptr;

    g_autofree gchar *value = g_key_file_get_string(this->gKeyFile.get(),
                                                    section.toLocal8Bit().constData(),
                                                    key.toLocal8Bit().constData(),
                                                    &gErr);

    if (gErr != nullptr) {
        return LINGLONG_ERR("g_key_file_get_string", gErr);
    }

    return value;
}

} // namespace linglong::utils::xdg
