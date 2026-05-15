/*
 * SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include <algorithm>
#include <cctype>
#include <string_view>
#include <vector>

namespace linglong::utils {

inline bool isValidXdgDesktopPortalId(std::string_view appid) noexcept
{
    std::vector<std::string_view> segments;

    size_t start = 0;
    for (size_t i = 0; i < appid.size(); ++i) {
        if (appid[i] == '.') {
            segments.push_back(appid.substr(start, i - start));
            start = i + 1;
        }
    }
    segments.push_back(appid.substr(start));

    if (segments.size() < 2) {
        return false;
    }

    for (size_t i = 0; i + 1 < segments.size(); ++i) {
        if (segments[i].empty()) {
            return false;
        }
        if (!std::all_of(segments[i].begin(), segments[i].end(), [](char c) {
                return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
            })) {
            return false;
        }
    }

    const auto &last = segments.back();
    if (last.empty()) {
        return false;
    }
    return std::all_of(last.begin(), last.end(), [](char c) {
        return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-';
    });
}

} // namespace linglong::utils
