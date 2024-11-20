// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linglong/utils/error/error.h"

#include <QStandardPaths>

#include <cstdlib>
#include <filesystem>
#include <string>

namespace linglong::utils::xdg {

inline utils::error::Result<std::string> appDataDir(const std::string &appID)
{
    LINGLONG_TRACE("get app data dir");
    auto *home = std::getenv("HOME"); // NOLINT
    if (home == nullptr) {
        return LINGLONG_ERR("HOME is not set");
    }

    return std::string(home) + "/.linglong/" + appID;
}

inline std::vector<std::string> userDirs()
{
    static std::vector<QStandardPaths::StandardLocation> locs{
        QStandardPaths::DesktopLocation,   QStandardPaths::MoviesLocation,
        QStandardPaths::MusicLocation,     QStandardPaths::PicturesLocation,
        QStandardPaths::DocumentsLocation, QStandardPaths::DownloadLocation
    };

    std::vector<std::string> dir(locs.size());
    for (auto loc : locs) {
        if (auto path = QStandardPaths::writableLocation(loc).toStdString(); !path.empty()) {
            dir.emplace_back(std::move(path));
        }
    }

    return dir;
}

} // namespace linglong::utils::xdg
