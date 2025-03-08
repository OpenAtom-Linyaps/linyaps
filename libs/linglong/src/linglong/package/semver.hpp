/*
SPDX-FileCopyrightText: 2024 Peter Csajtai <peter.csajtai@outlook.com>
SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: MIT

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#ifndef Z4KN4FEIN_SEMVER_H
#define Z4KN4FEIN_SEMVER_H

#ifndef SEMVER_MODULE
#  include "linglong/package/versionv2.h"

#  include <QRegularExpression>

#  include <iostream>
#  include <ostream>
#  include <regex>
#  include <string>
#  include <utility>
#  include <vector>

// conditionally include <format> and its dependency <string_view> for C++20
#  ifdef __cpp_lib_format
#    if __cpp_lib_format >= 201907L
#      include <format>
#      include <string_view>
#    endif
#  endif
#endif

namespace semver {
const std::string default_prerelease_part = "0";
const std::string numbers = "0123456789";
const std::string prerelease_allowed_chars =
  "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-";
const QRegularExpression version_pattern(
  R"(^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)(?:-((?:0|[1-9]\d*|\d*[a-zA-Z-][0-9a-zA-Z-]*)(?:\.(?:0|[1-9]\d*|\d*[a-zA-Z-][0-9a-zA-Z-]*))*))?(?:\+([0-9a-zA-Z-]+(?:\.[0-9a-zA-Z-]+)*))?$)");
const QRegularExpression loose_version_pattern(
  R"(^v?(0|[1-9]\d*)\.(0|[1-9]\d*)(?:\.(0|[1-9]\d*))?(?:-((?:0|[1-9]\d*|\d*[a-zA-Z-][0-9a-zA-Z-]*)(?:\.(?:0|[1-9]\d*|\d*[a-zA-Z-][0-9a-zA-Z-]*))*))?(?:\+([0-9a-zA-Z-]+(?:\.[0-9a-zA-Z-]+)*))?$)");

struct semver_exception : public std::runtime_error
{
    explicit semver_exception(const std::string &message)
        : std::runtime_error(message)
    {
    }
};

inline uint64_t parse_numeric_part(const std::string &version_part)
{
    return static_cast<uint64_t>(std::stoull(version_part));
}

inline std::vector<std::string> split(const std::string &text, const char &delimiter)
{
    std::size_t pos_start = 0, pos_end, delim_len = 1;
    std::string current;
    std::vector<std::string> result;

    while ((pos_end = text.find(delimiter, pos_start)) != std::string::npos) {
        current = text.substr(pos_start, pos_end - pos_start);
        pos_start = pos_end + delim_len;
        result.push_back(current);
    }

    result.push_back(text.substr(pos_start));
    return result;
}

inline bool is_numeric(const std::string &text)
{
    return text.find_first_not_of(numbers) == std::string::npos;
}

inline bool is_valid_prerelease(const std::string &text)
{
    return text.find_first_not_of(numbers + prerelease_allowed_chars) == std::string::npos;
}

class prerelease_part
{
private:
    bool m_numeric = false;
    std::string m_value;
    uint64_t m_numeric_value;

public:
    explicit prerelease_part(const std::string &part)
    {
        if (part.empty()) {
            throw semver_exception("Pre-release identity contains an empty part.");
        }

        if (is_numeric(part)) {
            if (part.size() > 1 && part[0] == '0') {
                throw semver_exception("Pre-release part '" + part
                                       + "' is numeric but contains a leading zero.");
            }
            m_numeric_value = parse_numeric_part(part);
            m_numeric = true;
        }
        if (!is_valid_prerelease(part)) {
            throw semver_exception("Pre-release part '" + part
                                   + "' contains an invalid character.");
        }
        m_value = part;
    }

    [[nodiscard]] bool numeric() const { return m_numeric; }

    [[nodiscard]] std::string value() const { return m_value; }

    [[nodiscard]] uint64_t numeric_value() const { return m_numeric_value; }

    [[nodiscard]] int compare(const prerelease_part &other) const
    {
        if (m_numeric && !other.m_numeric)
            return -1;
        if (!m_numeric && other.m_numeric)
            return 1;
        if (m_numeric) {
            return (m_numeric_value < other.m_numeric_value)
              ? -1
              : (m_numeric_value > other.m_numeric_value);
        }
        return (m_value < other.m_value) ? -1 : (m_value > other.m_value);
    }
};

class prerelease_descriptor
{
private:
    std::vector<prerelease_part> m_parts;
    std::string prerelease_str;

    explicit prerelease_descriptor(const std::vector<prerelease_part> &parts)
        : m_parts(parts)
    {
        if (parts.empty())
            prerelease_str = "";
        for (const auto &part : parts) {
            if (!prerelease_str.empty())
                prerelease_str += ".";
            prerelease_str += part.value();
        }
    }

public:
    [[nodiscard]] std::string str() const { return prerelease_str; }

    [[nodiscard]] bool is_empty() const { return m_parts.empty(); }

    [[nodiscard]] std::string identity() const
    {
        if (is_empty())
            return "";
        return m_parts.front().value();
    }

    [[nodiscard]] prerelease_descriptor increment() const
    {
        std::vector<prerelease_part> new_parts = (m_parts);
        size_t last_numeric_index = 0;
        bool last_numeric_index_found = false;
        for (size_t i = 0; i < new_parts.size(); ++i) {
            if (new_parts[i].numeric()) {
                last_numeric_index = i;
                last_numeric_index_found = true;
            }
        }
        if (last_numeric_index_found) {
            prerelease_part last = new_parts[last_numeric_index];
            new_parts[last_numeric_index] =
              prerelease_part(std::to_string(last.numeric_value() + 1));
        } else {
            new_parts.emplace_back(default_prerelease_part);
        }
        return prerelease_descriptor(new_parts);
    }

    [[nodiscard]] int compare(const prerelease_descriptor &other) const
    {
        auto this_size = m_parts.size();
        auto other_size = other.m_parts.size();

        auto count = std::min(this_size, other_size);
        for (size_t i = 0; i < count; ++i) {
            int cmp = m_parts[i].compare(other.m_parts[i]);
            if (cmp != 0)
                return cmp;
        }
        return (this_size < other_size) ? -1 : (this_size > other_size);
    }

    bool operator<(const prerelease_descriptor &other) const { return compare(other) == -1; }

    bool operator>(const prerelease_descriptor &other) const { return (other < *this); }

    bool operator==(const prerelease_descriptor &other) const
    {
        return prerelease_str == other.prerelease_str;
    }

    bool operator!=(const prerelease_descriptor &other) const
    {
        return prerelease_str != other.prerelease_str;
    }

    static prerelease_descriptor parse(const std::string &prerelease_part_str)
    {
        if (prerelease_part_str.empty())
            return empty();
        std::vector<prerelease_part> prerelease_parts;
        std::vector<std::string> parts = split(prerelease_part_str, '.');
        for (auto &part : parts) {
            prerelease_parts.emplace_back(part);
        }
        return prerelease_descriptor(prerelease_parts);
    }

    static prerelease_descriptor empty() { return prerelease_descriptor({}); }

    static prerelease_descriptor initial()
    {
        return prerelease_descriptor::parse(default_prerelease_part);
    }
};

enum inc { major, minor, patch, prerelease, security };

class version
{
private:
    uint64_t m_major;
    uint64_t m_minor;
    uint64_t m_patch;
    prerelease_descriptor m_prerelease;
    std::string m_build_meta;
    uint64_t m_security;
    bool m_has_patch;

    [[nodiscard]] int compare(const version &other) const
    {
        if (m_major > other.m_major)
            return 1;
        if (m_major < other.m_major)
            return -1;
        if (m_minor > other.m_minor)
            return 1;
        if (m_minor < other.m_minor)
            return -1;
        if (m_patch > other.m_patch)
            return 1;
        if (m_patch < other.m_patch)
            return -1;
        if (!m_prerelease.is_empty() && other.m_prerelease.is_empty())
            return -1;
        if (m_prerelease.is_empty() && !other.m_prerelease.is_empty())
            return 1;
        if (!m_prerelease.is_empty() && !other.m_prerelease.is_empty()) {
            int prerelease_cmp = m_prerelease.compare(other.m_prerelease);
            if (prerelease_cmp != 0)
                return prerelease_cmp;
        }

        if (m_security > other.m_security)
            return 1;
        if (m_security < other.m_security)
            return -1;
        return 0;
    }

public:
    explicit version(uint64_t major = 0,
                     uint64_t minor = 0,
                     uint64_t patch = 0,
                     const std::string &prerelease = "",
                     std::string build_meta = "",
                     uint64_t security = 0,
                     bool has_patch = false)
        : m_major{ major }
        , m_minor{ minor }
        , m_patch{ patch }
        , m_prerelease{ prerelease_descriptor::parse(prerelease) }
        , m_build_meta{ std::move(build_meta) }
        , m_security{ security }
        , m_has_patch{ has_patch }
    {
    }

    [[nodiscard]] uint64_t major() const { return m_major; }

    [[nodiscard]] uint64_t minor() const { return m_minor; }

    [[nodiscard]] uint64_t patch() const { return m_patch; }

    [[nodiscard]] std::string prerelease() const { return m_prerelease.str(); }

    [[nodiscard]] std::string build_meta() const { return m_build_meta; }

    [[nodiscard]] uint64_t security() const { return m_security; }

    [[nodiscard]] bool is_prerelease() const { return !m_prerelease.is_empty(); }

    [[nodiscard]] bool is_stable() const { return m_major > 0 && m_prerelease.is_empty(); }

    [[nodiscard]] bool has_patch() const { return m_has_patch; }

    [[nodiscard]] std::string str() const
    {
        std::string result =
          std::to_string(m_major) + "." + std::to_string(m_minor) + "." + std::to_string(m_patch);
        if (!m_prerelease.is_empty())
            result += "-" + m_prerelease.str();
        if (!m_build_meta.empty())
            result += "+" + m_build_meta;
        return result;
    }

    [[nodiscard]] version without_suffixes() const { return version(m_major, m_minor, m_patch); }

    [[nodiscard]] version next_major(const std::string &prerelease = "") const
    {
        return version(m_major + 1, 0, 0, prerelease, "", 0);
    }

    [[nodiscard]] version next_minor(const std::string &prerelease = "") const
    {
        return version(m_major, m_minor + 1, 0, prerelease, "", 0);
    }

    [[nodiscard]] version next_patch(const std::string &prerelease = "") const
    {
        return version(m_major,
                       m_minor,
                       (!is_prerelease() || !prerelease.empty() ? m_patch + 1 : m_patch),
                       prerelease,
                       "",
                       0);
    }

    [[nodiscard]] version next_prerelease(const std::string &prerelease = "") const
    {
        std::string pre = default_prerelease_part;
        if (!prerelease.empty()) {
            pre = is_prerelease() && m_prerelease.identity() == prerelease
              ? m_prerelease.increment().str()
              : prerelease;
        } else if (prerelease.empty() && is_prerelease()) {
            pre = m_prerelease.increment().str();
        }
        return version(m_major, m_minor, is_prerelease() ? m_patch : m_patch + 1, pre, "", 0);
    }

    [[nodiscard]] version next_security(const std::string &prerelease = "") const
    {
        std::string build_meta = m_build_meta;
        if (m_security == 0) {
            if (build_meta.empty()) {
                build_meta = "security.1";
            } else {
                build_meta += ".security.1";
            }
        } else {
            build_meta = build_meta.substr(0, build_meta.find_last_of(".") + 1)
              + std::to_string(m_security + 1);
        }
        return version(m_major,
                       m_minor,
                       m_patch,
                       prerelease.empty() ? m_prerelease.str() : prerelease,
                       build_meta,
                       m_security + 1);
    }

    [[nodiscard]] version increment(inc by, const std::string &prerelease = "") const
    {
        switch (by) {
        case semver::major:
            return next_major(prerelease);
        case semver::minor:
            return next_minor(prerelease);
        case semver::patch:
            return next_patch(prerelease);
        case semver::prerelease:
            return next_prerelease(prerelease);
        case semver::security:
            return next_security(prerelease);
        default:
            throw semver_exception("Invalid 'by' parameter in 'increment()' function.");
        }
    }

    bool operator<(const version &other) const { return compare(other) == -1; }

    bool operator<=(const version &other) const { return compare(other) <= 0; }

    bool operator>(const version &other) const { return compare(other) == 1; }

    bool operator>=(const version &other) const { return compare(other) >= 0; }

    bool operator==(const version &other) const { return compare(other) == 0; }

    bool operator!=(const version &other) const { return compare(other) != 0; }

// conditionally provide three-way operator for C++20
#ifdef __cpp_impl_three_way_comparison
#  if __cpp_impl_three_way_comparison >= 201907L

    auto operator<=>(const version &other) const { return compare(other); }

#  endif
#endif

    static version parse(const std::string &version_str, bool strict = true)
    {
        QRegularExpression regex(strict ? version_pattern : loose_version_pattern);
        QRegularExpressionMatch match = regex.match(QString::fromStdString(version_str));

        if (!match.hasMatch()) {
            throw semver_exception("Invalid version: " + version_str);
        }

        try {
            uint64_t major;
            uint64_t minor;
            uint64_t patch;
            std::string prerelease;
            std::string build_meta;
            uint64_t security = 0;

            if (strict && !match.captured(1).isNull() && !match.captured(2).isNull()
                && !match.captured(3).isNull()) {
                major = parse_numeric_part(match.captured(1).toStdString());
                minor = parse_numeric_part(match.captured(2).toStdString());
                patch = parse_numeric_part(match.captured(3).toStdString());
            } else if (!strict && !match.captured(1).isNull() && !match.captured(2).isNull()) {
                major = parse_numeric_part(match.captured(1).toStdString());
                minor = !match.captured(2).isNull()
                  ? parse_numeric_part(match.captured(2).toStdString())
                  : 0;
                patch = !match.captured(3).isNull()
                  ? parse_numeric_part(match.captured(3).toStdString())
                  : 0;
            } else {
                throw semver_exception("Invalid version: " + version_str);
            }

            if (!match.captured(4).isNull()) {
                prerelease = match.captured(4).toStdString();
            }
            if (!match.captured(5).isNull()) {
                build_meta = match.captured(5).toStdString();
                size_t security_pos = build_meta.find("security.");
                if (security_pos != std::string::npos) {
                    std::string security_part = build_meta.substr(security_pos + 9);

                    if (security_part.empty()) {
                        throw semver_exception("Invalid security format: security. is empty");
                    }

                    if (security_part.find('-') != std::string::npos) {
                        throw semver_exception(
                          "Invalid security format: security value cannot be negative");
                    }

                    if (security_part.size() > 1 && security_part[0] == '0') {
                        throw semver_exception(
                          "Invalid security format: security value cannot have leading zeros");
                    }

                    if (security_part.find_first_not_of("0123456789") != std::string::npos) {
                        throw semver_exception("Invalid security format: " + security_part);
                    }

                    uint64_t security_value = parse_numeric_part(security_part);
                    if (security_value == 0) {
                        throw semver_exception(
                          "Invalid security format: security value must be greater than 0");
                    }

                    security = security_value;
                }
            }

            return version(major, minor, patch, prerelease, build_meta, security, !match.captured(3).isNull());
        } catch (std::exception &exception) {
            throw semver_exception("Version parse error: " + std::string(exception.what()));
        }
    }
};

inline std::ostream &operator<<(std::ostream &str, const version &version)
{
    for (const auto s : version.str())
        str.put(s);
    return str;
}

namespace literals {
inline version operator""_v(const char *text, std::size_t length)
{
    return version::parse(std::string(text, length));
}

inline version operator""_lv(const char *text, std::size_t length)
{
    return version::parse(std::string(text, length), false);
}
} // namespace literals
} // namespace semver

// conditionally provide formatter for C++20
#ifdef __cpp_lib_format
#  if __cpp_lib_format >= 201907L

template<class CharT>
struct std::formatter<semver::version, CharT> : std::formatter<std::string_view>
{
    template<class FormatContext>
    auto format(const semver::version &version, FormatContext &ctx) const
    {
        return std::formatter<std::string_view>::format(version.str(), ctx);
    }
};

#  endif
#endif

#endif // Z4KN4FEIN_SEMVER_H
