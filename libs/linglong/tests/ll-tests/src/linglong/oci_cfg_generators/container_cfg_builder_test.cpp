// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "../../common/tempdir.h"
#include "linglong/common/strings.h"
#include "linglong/oci-cfg-generators/container_cfg_builder.h"

#include <algorithm>
#include <filesystem>
#include <optional>
#include <string>

using namespace linglong;

namespace {

std::optional<std::string> getEnvValue(const ocppi::runtime::config::types::Config &config,
                                       const std::string &key)
{
    if (!config.process.has_value() || !config.process->env.has_value()) {
        return std::nullopt;
    }

    const std::string prefix = key + "=";
    for (const auto &entry : *config.process->env) {
        if (entry.size() >= prefix.size() && entry.compare(0, prefix.size(), prefix) == 0) {
            return entry.substr(prefix.size());
        }
    }
    return std::nullopt;
}

bool containsPart(std::string_view value, std::string_view part)
{
    auto parts = common::strings::split(value, ':', common::strings::splitOption::SkipEmpty);
    return std::find(parts.begin(), parts.end(), part) != parts.end();
}

std::ptrdiff_t countPart(std::string_view value, std::string_view part)
{
    auto parts = common::strings::split(value, ':', common::strings::splitOption::SkipEmpty);
    return std::count(parts.begin(), parts.end(), part);
}

} // namespace

// Regression test for issue #1457: the Vulkan ICD loader discovers driver
// manifests by scanning each XDG_DATA_DIRS entry for a "vulkan/icd.d"
// subdirectory. When XDG_DATA_DIRS is not present in the container environment,
// build() must still expose the conventional system data directories so that
// system provided drivers (e.g. the Mesa Turnip/freedreno Vulkan driver under
// /usr/share/vulkan/icd.d) remain discoverable.
TEST(ContainerCfgBuilderEnvTest, XdgDataDirsUnsetGetsSystemDefaults)
{
    TempDir tmp;
    ASSERT_TRUE(tmp.isValid());
    std::filesystem::create_directories(tmp.path() / "bundle");

    generator::ContainerCfgBuilder builder;
    builder.setAppId("org.deepin.base")
      .setBasePath(tmp.path() / "base")
      .setBundlePath(tmp.path() / "bundle")
      .disablePatch();

    auto result = builder.build();
    ASSERT_TRUE(result.has_value()) << result.error().message();

    auto xdg = getEnvValue(builder.getConfig(), "XDG_DATA_DIRS");
    ASSERT_TRUE(xdg.has_value());
    EXPECT_TRUE(containsPart(*xdg, "/usr/share"));
    EXPECT_TRUE(containsPart(*xdg, "/usr/local/share"));
}

// Regression test for issue #1457: a host XDG_DATA_DIRS that only points at
// Linyaps specific paths may be forwarded into the container, overriding the
// Vulkan loader default search path. The forwarded value must be preserved
// while the standard system data directories are appended so that the loader
// still finds /usr/share/vulkan/icd.d.
TEST(ContainerCfgBuilderEnvTest, XdgDataDirsForwardedHostValueKeepsSystemDefaults)
{
    TempDir tmp;
    ASSERT_TRUE(tmp.isValid());
    std::filesystem::create_directories(tmp.path() / "bundle");

    generator::ContainerCfgBuilder builder;
    builder.setAppId("org.deepin.base")
      .setBasePath(tmp.path() / "base")
      .setBundlePath(tmp.path() / "bundle")
      .disablePatch()
      .appendEnv("XDG_DATA_DIRS", "/var/lib/linglong/entries/share");

    auto result = builder.build();
    ASSERT_TRUE(result.has_value()) << result.error().message();

    auto xdg = getEnvValue(builder.getConfig(), "XDG_DATA_DIRS");
    ASSERT_TRUE(xdg.has_value());
    EXPECT_TRUE(containsPart(*xdg, "/var/lib/linglong/entries/share"));
    EXPECT_TRUE(containsPart(*xdg, "/usr/share"));
    EXPECT_TRUE(containsPart(*xdg, "/usr/local/share"));
}

TEST(ContainerCfgBuilderEnvTest, XdgDataDirsDoesNotDuplicateSystemDefaults)
{
    TempDir tmp;
    ASSERT_TRUE(tmp.isValid());
    std::filesystem::create_directories(tmp.path() / "bundle");

    generator::ContainerCfgBuilder builder;
    builder.setAppId("org.deepin.base")
      .setBasePath(tmp.path() / "base")
      .setBundlePath(tmp.path() / "bundle")
      .disablePatch()
      .appendEnv("XDG_DATA_DIRS", "/usr/share:/usr/local/share");

    auto result = builder.build();
    ASSERT_TRUE(result.has_value()) << result.error().message();

    auto xdg = getEnvValue(builder.getConfig(), "XDG_DATA_DIRS");
    ASSERT_TRUE(xdg.has_value());
    EXPECT_EQ(*xdg, "/usr/share:/usr/local/share");
    EXPECT_EQ(countPart(*xdg, "/usr/share"), 1);
    EXPECT_EQ(countPart(*xdg, "/usr/local/share"), 1);
}
