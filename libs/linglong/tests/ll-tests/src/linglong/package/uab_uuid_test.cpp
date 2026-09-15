// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "uab_uuid.h"

#include <gtest/gtest.h>

#include <array>
#include <string_view>

namespace {

TEST(UabUuidTest, AcceptsCanonicalVersion4Uuid)
{
    EXPECT_TRUE(linglong::uab::isUabUuidV4("b2f33c7b-615c-4d7d-9181-e1a22010a749"));
}

TEST(UabUuidTest, RejectsUnsafeOrNoncanonicalValues)
{
    constexpr std::array<std::string_view, 8> invalidUuids{
        "",
        "/tmp/outside-uab-runtime",
        "../../outside-uab-runtime",
        "b2f33c7b-615c-4d7d-7181-e1a22010a749",
        "b2f33c7b-615c-4d7d-c181-e1a22010a749",
        "b2f33c7b-615c-4d7d-9181-e1a22010a74g",
        "{b2f33c7b-615c-4d7d-9181-e1a22010a749}",
        "b2f33c7b615c4d7d9181e1a22010a749",
    };

    for (const auto uuid : invalidUuids) {
        EXPECT_FALSE(linglong::uab::isUabUuidV4(uuid)) << uuid;
    }
}

} // namespace
