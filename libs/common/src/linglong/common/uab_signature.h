// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string_view>

namespace linglong::common::uab {

inline constexpr std::string_view signatureSection{ ".note.uab.sig" };
inline constexpr std::string_view metaSection{ "linglong.meta" };
inline constexpr std::size_t digestSize = 64;
inline constexpr std::uint32_t signatureNoteType = 1;

constexpr std::size_t noteAlign(std::size_t size) noexcept
{
    return (size + 3U) & ~std::size_t{ 3U };
}

struct NoteHeader
{
    std::uint32_t nameSize;
    std::uint32_t descriptorSize;
    std::uint32_t type;
};

template <std::size_t NameSize>
struct SignatureNote
{
    NoteHeader header;
    std::array<char, noteAlign(NameSize)> name;
    std::array<char, digestSize> digest;
};

template <std::size_t NameSize>
constexpr SignatureNote<NameSize> makeSignatureNote(const char (&name)[NameSize]) noexcept
{
    SignatureNote<NameSize> note{};
    note.header = { NameSize, digestSize, signatureNoteType };
    for (std::size_t i = 0; i < NameSize; ++i) {
        note.name[i] = name[i];
    }
    note.digest[0] = '!';
    return note;
}

using MetaSignatureNote = SignatureNote<sizeof("linglong.meta")>;

inline constexpr MetaSignatureNote signatureNote = makeSignatureNote("linglong.meta");
inline constexpr std::size_t digestOffset = offsetof(MetaSignatureNote, digest);

inline bool isDigest(std::string_view digest) noexcept
{
    return digest.size() == digestSize
      && std::all_of(digest.cbegin(), digest.cend(), [](char value) noexcept {
               return (value >= '0' && value <= '9') || (value >= 'a' && value <= 'f');
           });
}

inline std::optional<std::string_view> parseSignatureNote(std::string_view data) noexcept
{
    if (data.size() != sizeof(MetaSignatureNote)) {
        return std::nullopt;
    }

    NoteHeader header{};
    std::memcpy(&header, data.data(), sizeof(header));
    if (header.nameSize != metaSection.size() + 1 || header.descriptorSize != digestSize
        || header.type != signatureNoteType) {
        return std::nullopt;
    }

    const auto nameOffset = sizeof(NoteHeader);
    const auto name = data.substr(nameOffset, header.nameSize);
    if (name.substr(0, name.size() - 1) != metaSection || name.back() != '\0') {
        return std::nullopt;
    }

    const auto alignedNameSize = noteAlign(header.nameSize);
    const auto padding =
      data.substr(nameOffset + header.nameSize, alignedNameSize - header.nameSize);
    if (!std::all_of(padding.cbegin(), padding.cend(), [](char value) {
            return value == '\0';
        })) {
        return std::nullopt;
    }

    return data.substr(digestOffset, digestSize);
}

static_assert(sizeof(NoteHeader) == 12);
static_assert(digestOffset == 28);
static_assert(sizeof(MetaSignatureNote) == 92);

} // namespace linglong::common::uab
