/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_UTILS_XDG_DESKTOP_ENTRY_H_
#define LINGLONG_UTILS_XDG_DESKTOP_ENTRY_H_

#include "linglong/utils/error/error.h"
#include "linglong/utils/finally/finally.h"

#include <glib.h>
#include <tl/expected.hpp>

#include <QFile>
#include <QString>

#include <type_traits>

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
        auto desktopEntryFile = QFile(filePath);
        if (!desktopEntryFile.exists()) {
            return Err(ENOENT, "no such desktop entry file");
        }

        GError *gErr = nullptr;
        auto _ = linglong::utils::finally::finally([&gErr]() {
            if (gErr != nullptr)
                g_error_free(gErr);
        });

        DesktopEntry entry;
        if (!g_key_file_load_from_file(entry.gKeyFile.get(),
                                       filePath.toLocal8Bit().constData(),
                                       G_KEY_FILE_KEEP_TRANSLATIONS,
                                       &gErr)) {
            return Err(gErr->code, gErr->message);
        }

        return entry;
    }

private:
    DesktopEntry()
        : gKeyFile(std::unique_ptr<GKeyFile, decltype(&g_key_file_free)>(g_key_file_new(),
                                                                         g_key_file_free)){};

    // types
public:
    using SectionName = QString;

    // statics data members
public:
    static const SectionName MainSection;

    // data members
private:
    std::unique_ptr<GKeyFile, decltype(&g_key_file_free)> gKeyFile;

    // methods
public:
    template<typename Value>
    void setValue(const QString &key,
                  const Value &value,
                  const SectionName &section = MainSection) = delete;

    template<>
    void setValue(const QString &key, const QString &value, const SectionName &section)
    {
        g_key_file_set_string(this->gKeyFile.get(),
                              section.toLocal8Bit().constData(),
                              key.toLocal8Bit().constData(),
                              value.toLocal8Bit().constData());
        return;
    }

    template<typename Value>
    auto getValue(const QString &key, const SectionName &section = MainSection) const
            -> error::Result<Value> = delete;

    template<>
    [[nodiscard]] auto getValue(const QString &key, const SectionName &section) const
            -> error::Result<QString>
    {
        GError *gErr = nullptr;
        auto _ = linglong::utils::finally::finally([&gErr]() {
            if (gErr != nullptr)
                g_error_free(gErr);
        });

        auto value = g_key_file_get_string(this->gKeyFile.get(),
                                           section.toLocal8Bit().constData(),
                                           key.toLocal8Bit().constData(),
                                           &gErr);
        auto _1 = finally::finally([&value]() {
            g_free(value);
        });

        if (gErr) {
            return Err(gErr->code, gErr->message);
        }

        return value;
    }

    auto groups() -> QStringList
    {
        gsize length = 0;
        auto groups = g_key_file_get_groups(this->gKeyFile.get(), &length);
        auto _ = finally::finally([&groups]() {
            g_strfreev(groups);
        });

        QStringList result;
        for (gsize i = 0; i < length; i++) {
            auto group = groups[i]; // NOLINT
            result << group;
        }

        return result;
    }

    auto saveToFile(const QString &filepath) -> error::Result<void>
    {
        GError *gErr = nullptr;
        auto _ = linglong::utils::finally::finally([&gErr]() {
            if (gErr != nullptr)
                g_error_free(gErr);
        });

        if (!g_key_file_save_to_file(this->gKeyFile.get(),
                                     filepath.toLocal8Bit().constData(),
                                     &gErr)) {
            return Err(gErr->code, gErr->message);
        }

        return Ok;
    }
};

} // namespace linglong::utils::xdg

#endif
