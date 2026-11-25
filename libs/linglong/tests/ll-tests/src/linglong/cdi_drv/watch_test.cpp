// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "common/tempdir.h"
#include "linglong/cdi_drv/config.h"
#include "linglong/cdi_drv/refresh.h"
#include "linglong/cdi_drv/watch.h"
#include "linglong/utils/file.h"

#include <nlohmann/json.hpp>

#include <chrono>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include <sys/stat.h>

namespace {

namespace fs = std::filesystem;
using linglong::utils::readFile;
using linglong::utils::writeFile;
using nlohmann::json;
using testing::HasSubstr;
using testing::Not;

class CdiDrvWatchTest : public testing::Test
{
protected:
    void SetUp() override
    {
        ASSERT_TRUE(fs::create_directory(configDir));
        auto result = writeFile(systemctl, R"sh(#!/bin/sh
printf '%s\n' "$*" >> "${0%/*}/calls"
if [ -e "${0%/*}/fail" ]; then
    exit 1
fi
if [ -e "${0%/*}/fail-restart" ] && [ "$2" = restart ]; then
    exit 1
fi
)sh");
        ASSERT_TRUE(result);
        fs::permissions(systemctl, fs::perms::owner_all);
    }

    testing::AssertionResult configure(const std::string &name,
                                       const std::vector<std::string> &paths) const
    {
        // Even a generator that cannot run must get its dependency watches.
        const json config{ { "version", "1" },
                           { "generator", "command" },
                           { "argv", { "/missing/generator" } },
                           { "watchPathChanged", paths } };
        auto written = writeFile(configDir / (name + ".json"), config.dump());
        if (!written) {
            return testing::AssertionFailure() << written.error().message();
        }
        return testing::AssertionSuccess();
    }

    testing::AssertionResult sync(const linglong::cdi_drv::RefreshRequests &requests = { true,
                                                                                         {} }) const
    {
        auto names = linglong::cdi_drv::configNames(configDir, requests);
        if (!names) {
            return testing::AssertionFailure() << names.error().message();
        }
        linglong::cdi_drv::WatchFiles watches;
        for (const auto &name : *names) {
            auto config = linglong::cdi_drv::loadConfig(configDir / (name + ".json"));
            if (!config) {
                return testing::AssertionFailure() << config.error().message();
            }
            if (!*config) {
                continue;
            }
            auto watch = linglong::cdi_drv::generateWatch(name, **config);
            if (watch && *watch) {
                watches.emplace(name, std::move(**watch));
            }
        }
        auto result =
          linglong::cdi_drv::syncWatches(watches, requests, { unitDir, systemctl.string() });
        if (!result) {
            return testing::AssertionFailure() << result.error().message();
        }
        return testing::AssertionSuccess();
    }

    TempDir directory;
    const fs::path configDir{ directory.path() / "config" };
    const fs::path unitDir{ directory.path() / "units" };
    const fs::path systemctl{ directory.path() / "systemctl" };
    const fs::path calls{ directory.path() / "calls" };
};

TEST_F(CdiDrvWatchTest, GeneratesOneWatchPerConfigurationWithoutRunningGenerators)
{
    ASSERT_TRUE(configure("nvidia", { "/usr/bin/nvidia-ctk", "/lib/modules/%v/modules.dep" }));
    ASSERT_TRUE(configure("intel", { "/usr/lib/driver with spaces.so" }));
    ASSERT_TRUE(sync());

    auto content = readFile(unitDir / "ll-cdi-drv-watch@nvidia.path");
    ASSERT_TRUE(content);
    EXPECT_THAT(*content, HasSubstr("Unit=ll-cdi-drv-request@nvidia.service\n"));
    EXPECT_THAT(*content, HasSubstr("PathChanged=/usr/bin/nvidia-ctk\n"));
    EXPECT_THAT(*content, HasSubstr("PathChanged=/lib/modules/%v/modules.dep\n"));
    auto intel = readFile(unitDir / "ll-cdi-drv-watch@intel.path");
    ASSERT_TRUE(intel);
    EXPECT_THAT(*intel, HasSubstr("PathChanged=/usr/lib/driver with spaces.so\n"));
    auto commands = readFile(calls);
    ASSERT_TRUE(commands);
    EXPECT_EQ(*commands,
              "daemon-reload\n"
              "--no-block restart -- ll-cdi-drv-watch@intel.path ll-cdi-drv-watch@nvidia.path\n");
}

TEST_F(CdiDrvWatchTest, RewritesUnchangedDefinitionsAndRestartsWatches)
{
    ASSERT_TRUE(configure("nvidia", { "/usr/bin/nvidia-ctk" }));
    ASSERT_TRUE(sync());
    const auto unit = unitDir / "ll-cdi-drv-watch@nvidia.path";
    struct stat before{};
    ASSERT_EQ(::stat(unit.c_str(), &before), 0);
    ASSERT_TRUE(writeFile(calls, ""));
    ASSERT_TRUE(sync());
    struct stat after{};
    ASSERT_EQ(::stat(unit.c_str(), &after), 0);
    EXPECT_NE(before.st_ino, after.st_ino);
    auto commands = readFile(calls);
    ASSERT_TRUE(commands);
    EXPECT_EQ(*commands,
              "daemon-reload\n"
              "--no-block restart -- ll-cdi-drv-watch@nvidia.path\n");
}

TEST_F(CdiDrvWatchTest, RestartsAllWatchesInTheFullRefreshScope)
{
    ASSERT_TRUE(configure("nvidia", { "/usr/bin/nvidia-ctk" }));
    ASSERT_TRUE(configure("intel", { "/usr/lib/intel.so" }));
    ASSERT_TRUE(sync());
    ASSERT_TRUE(writeFile(calls, ""));
    ASSERT_TRUE(configure("nvidia", { "/usr/lib/nvidia.so" }));
    ASSERT_TRUE(sync());
    auto commands = readFile(calls);
    ASSERT_TRUE(commands);
    EXPECT_EQ(*commands,
              "daemon-reload\n"
              "--no-block restart -- ll-cdi-drv-watch@intel.path ll-cdi-drv-watch@nvidia.path\n");
    auto content = readFile(unitDir / "ll-cdi-drv-watch@nvidia.path");
    ASSERT_TRUE(content);
    EXPECT_THAT(*content, HasSubstr("PathChanged=/usr/lib/nvidia.so\n"));
    EXPECT_THAT(*content, Not(HasSubstr("/usr/bin/nvidia-ctk")));
}

TEST_F(CdiDrvWatchTest, StopsWatchesForDeletedAndClearedConfigurations)
{
    ASSERT_TRUE(configure("nvidia", { "/usr/bin/nvidia-ctk" }));
    ASSERT_TRUE(configure("intel", { "/usr/lib/intel.so" }));
    ASSERT_TRUE(sync());
    ASSERT_TRUE(writeFile(unitDir / "unrelated.path", "unrelated"));
    ASSERT_TRUE(writeFile(calls, ""));
    ASSERT_TRUE(fs::remove(configDir / "nvidia.json"));
    ASSERT_TRUE(configure("intel", {}));
    ASSERT_TRUE(sync());
    EXPECT_FALSE(fs::exists(unitDir / "ll-cdi-drv-watch@nvidia.path"));
    EXPECT_FALSE(fs::exists(unitDir / "ll-cdi-drv-watch@intel.path"));
    EXPECT_TRUE(fs::exists(unitDir / "unrelated.path"));
    auto commands = readFile(calls);
    ASSERT_TRUE(commands);
    EXPECT_EQ(*commands,
              "--no-block stop -- ll-cdi-drv-watch@intel.path ll-cdi-drv-watch@nvidia.path\n"
              "daemon-reload\n");
}

TEST_F(CdiDrvWatchTest, HandlesMissingConfigurationDirectoryAndSkipsUnrelatedEntries)
{
    ASSERT_TRUE(configure(".hidden", { "/usr/lib/hidden.so" }));
    ASSERT_TRUE(writeFile(configDir / "backup.json~", "invalid"));
    ASSERT_TRUE(fs::create_directory(configDir / "directory.json"));
    fs::create_symlink(configDir / ".hidden.json", configDir / "link.json");
    ASSERT_TRUE(sync());
    auto commands = readFile(calls);
    ASSERT_TRUE(commands);
    EXPECT_EQ(*commands, "daemon-reload\n");
    fs::remove_all(configDir);
    ASSERT_TRUE(sync());
    commands = readFile(calls);
    ASSERT_TRUE(commands);
    EXPECT_EQ(*commands, "daemon-reload\ndaemon-reload\n");
}

TEST_F(CdiDrvWatchTest, EscapesConfigurationNamesInBothUnitAndServiceNames)
{
    ASSERT_TRUE(configure("GPU-1.0_test", { "/usr/lib/driver.so" }));
    ASSERT_TRUE(sync({ false, { "GPU-1.0_test" } }));
    const std::string instance = R"(GPU\x2d1.0_test)";
    auto content = readFile(unitDir / ("ll-cdi-drv-watch@" + instance + ".path"));
    ASSERT_TRUE(content);
    EXPECT_THAT(*content, HasSubstr("Unit=ll-cdi-drv-request@" + instance + ".service\n"));
}

TEST_F(CdiDrvWatchTest, SkipsInvalidConfigurationNamesDuringDirectoryScan)
{
    ASSERT_TRUE(configure("GPU-1.0_test", { "/usr/lib/driver.so" }));
    for (const auto *name : { "gpu name", "gpu${FOO}", "gpu%", "gpu@1", "gpu:1", "显卡" }) {
        ASSERT_TRUE(configure(name, { "/usr/lib/driver.so" }));
        EXPECT_FALSE(linglong::cdi_drv::configNames(configDir, { false, { name } }));
    }
    auto names = linglong::cdi_drv::configNames(configDir, { true, {} });
    ASSERT_TRUE(names);
    EXPECT_THAT(*names, testing::ElementsAre("GPU-1.0_test"));
    ASSERT_TRUE(sync());
    EXPECT_TRUE(fs::is_regular_file(unitDir / R"(ll-cdi-drv-watch@GPU\x2d1.0_test.path)"));
}

TEST(CdiDrvConfigNameTest, ValidatesAllowedCharacters)
{
    for (const auto *name : { "nvidia", "GPU-0.1_test", "0", "_gpu", "-gpu" }) {
        EXPECT_TRUE(linglong::cdi_drv::validConfigName(name)) << name;
    }
    for (const auto &name :
         { std::string{}, std::string{ ".hidden" }, std::string{ "gpu\0x", 5 } }) {
        EXPECT_FALSE(linglong::cdi_drv::validConfigName(name));
    }
}

TEST_F(CdiDrvWatchTest, SkipsMalformedWatchPathsAndContinuesWithOtherConfigurations)
{
    ASSERT_TRUE(configure("valid", { "/usr/lib/valid.so" }));
    for (const auto &path : { "relative.so",
                              "/driver\n[Service]\nExecStart=/bin/false",
                              "/driver\rname",
                              "/trailing\\",
                              "/trailing " }) {
        ASSERT_TRUE(configure("invalid", { path }));
        ASSERT_TRUE(sync());
        EXPECT_FALSE(fs::exists(unitDir / "ll-cdi-drv-watch@invalid.path"));
        EXPECT_TRUE(fs::exists(unitDir / "ll-cdi-drv-watch@valid.path"));
    }
    ASSERT_TRUE(writeFile(configDir / "invalid.json", "not json"));
    ASSERT_TRUE(sync());
    ASSERT_TRUE(writeFile(configDir / "invalid.json", R"({"watchPathChanged":true})"));
    ASSERT_TRUE(sync());
}

TEST_F(CdiDrvWatchTest, RetriesManagerUpdatesAfterFailure)
{
    ASSERT_TRUE(configure("nvidia", { "/usr/bin/nvidia-ctk" }));
    const auto failure = directory.path() / "fail";
    ASSERT_TRUE(writeFile(failure, ""));
    EXPECT_FALSE(sync());
    EXPECT_TRUE(fs::exists(unitDir / "ll-cdi-drv-watch@nvidia.path"));
    ASSERT_TRUE(fs::remove(failure));
    ASSERT_TRUE(writeFile(calls, ""));
    ASSERT_TRUE(sync());
    auto commands = readFile(calls);
    ASSERT_TRUE(commands);
    EXPECT_EQ(*commands,
              "daemon-reload\n"
              "--no-block restart -- ll-cdi-drv-watch@nvidia.path\n");
}

TEST_F(CdiDrvWatchTest, KeepsExistingWatchesWhenConfigurationDirectoryCannotBeRead)
{
    ASSERT_TRUE(configure("nvidia", { "/usr/bin/nvidia-ctk" }));
    ASSERT_TRUE(sync());
    fs::remove_all(configDir);
    ASSERT_TRUE(writeFile(configDir, "not a directory"));
    ASSERT_TRUE(writeFile(calls, ""));
    EXPECT_FALSE(sync());
    EXPECT_TRUE(fs::exists(unitDir / "ll-cdi-drv-watch@nvidia.path"));
    auto commands = readFile(calls);
    ASSERT_TRUE(commands);
    EXPECT_TRUE(commands->empty());
}

TEST_F(CdiDrvWatchTest, NamedSynchronizationUpdatesOnlyRequestedConfigurations)
{
    ASSERT_TRUE(configure("nvidia", { "/usr/bin/nvidia-ctk" }));
    ASSERT_TRUE(configure("intel", { "/usr/lib/intel.so" }));
    ASSERT_TRUE(sync());
    const auto intelUnit = unitDir / "ll-cdi-drv-watch@intel.path";
    const auto oldIntel = readFile(intelUnit);
    ASSERT_TRUE(oldIntel);
    ASSERT_TRUE(writeFile(configDir / "intel.json", "not valid json"));
    ASSERT_TRUE(configure("nvidia", { "/usr/lib/nvidia.so" }));
    ASSERT_TRUE(configure("amd", { "/usr/lib/amd.so" }));
    ASSERT_TRUE(writeFile(calls, ""));
    ASSERT_TRUE(sync({ false, { "nvidia", "amd" } }));
    const auto intel = readFile(intelUnit);
    ASSERT_TRUE(intel);
    EXPECT_EQ(*intel, *oldIntel);
    auto content = readFile(unitDir / "ll-cdi-drv-watch@nvidia.path");
    ASSERT_TRUE(content);
    EXPECT_THAT(*content, HasSubstr("PathChanged=/usr/lib/nvidia.so\n"));
    EXPECT_TRUE(fs::exists(unitDir / "ll-cdi-drv-watch@amd.path"));
    auto commands = readFile(calls);
    ASSERT_TRUE(commands);
    EXPECT_EQ(*commands,
              "daemon-reload\n"
              "--no-block restart -- ll-cdi-drv-watch@amd.path ll-cdi-drv-watch@nvidia.path\n");
}

TEST_F(CdiDrvWatchTest, NamedSynchronizationRemovesOnlyDeletedOrClearedWatches)
{
    for (const bool deleted : { false, true }) {
        SCOPED_TRACE(deleted);
        ASSERT_TRUE(configure("nvidia", { "/usr/bin/nvidia-ctk" }));
        ASSERT_TRUE(configure("intel", { "/usr/lib/intel.so" }));
        ASSERT_TRUE(sync());
        if (deleted) {
            ASSERT_TRUE(fs::remove(configDir / "nvidia.json"));
        } else {
            ASSERT_TRUE(configure("nvidia", {}));
        }
        ASSERT_TRUE(writeFile(calls, ""));
        ASSERT_TRUE(sync({ false, { "nvidia" } }));
        EXPECT_FALSE(fs::exists(unitDir / "ll-cdi-drv-watch@nvidia.path"));
        EXPECT_TRUE(fs::exists(unitDir / "ll-cdi-drv-watch@intel.path"));
        auto commands = readFile(calls);
        ASSERT_TRUE(commands);
        EXPECT_EQ(*commands,
                  "--no-block stop -- ll-cdi-drv-watch@nvidia.path\n"
                  "daemon-reload\n");
    }
}

TEST_F(CdiDrvWatchTest, NamedSynchronizationRecoversOnlyTheRequestedWatchAfterFailure)
{
    ASSERT_TRUE(configure("nvidia", { "/usr/bin/nvidia-ctk" }));
    ASSERT_TRUE(configure("intel", { "/usr/lib/intel.so" }));
    ASSERT_TRUE(sync());
    ASSERT_TRUE(configure("nvidia", { "/usr/lib/nvidia.so" }));
    const auto failure = directory.path() / "fail";
    ASSERT_TRUE(writeFile(failure, ""));
    EXPECT_FALSE(sync({ false, { "nvidia" } }));
    ASSERT_TRUE(fs::remove(failure));
    ASSERT_TRUE(configure("intel", { "/usr/lib/intel-new.so" }));
    ASSERT_TRUE(writeFile(calls, ""));
    ASSERT_TRUE(sync({ false, { "intel" } }));
    auto commands = readFile(calls);
    ASSERT_TRUE(commands);
    EXPECT_THAT(*commands, Not(HasSubstr("nvidia")));
    ASSERT_TRUE(writeFile(calls, ""));
    ASSERT_TRUE(sync({ false, { "nvidia" } }));
    commands = readFile(calls);
    ASSERT_TRUE(commands);
    EXPECT_EQ(*commands,
              "daemon-reload\n"
              "--no-block restart -- ll-cdi-drv-watch@nvidia.path\n");
}

TEST_F(CdiDrvWatchTest, NamedRefreshUsesTheSameScopeForWatchesAndGeneration)
{
    ASSERT_TRUE(configure("nvidia", { "/usr/bin/nvidia-ctk" }));
    ASSERT_TRUE(configure("intel", { "/usr/lib/intel.so" }));
    const auto runtimeDir = directory.path() / "runtime";
    linglong::cdi_drv::RefreshOptions options{ configDir, runtimeDir, "nvidia" };
    options.watches = { unitDir, systemctl.string() };
    auto result = linglong::cdi_drv::refresh(options);
    ASSERT_TRUE(result) << result.error().message();
    EXPECT_TRUE(fs::exists(unitDir / "ll-cdi-drv-watch@nvidia.path"));
    EXPECT_FALSE(fs::exists(unitDir / "ll-cdi-drv-watch@intel.path"));
    ASSERT_TRUE(fs::remove(configDir / "nvidia.json"));
    ASSERT_TRUE(linglong::cdi_drv::requestRefresh(runtimeDir, "nvidia"));
    options.pending = true;
    options.idleTimeout = std::chrono::seconds::zero();
    options.name.clear();
    result = linglong::cdi_drv::refresh(options);
    ASSERT_TRUE(result) << result.error().message();
    EXPECT_FALSE(fs::exists(unitDir / "ll-cdi-drv-watch@nvidia.path"));
    EXPECT_FALSE(fs::exists(runtimeDir / "pending/refresh-single/nvidia"));
}

TEST_F(CdiDrvWatchTest, ConsumesRequestsSubmittedDuringSynchronizationInTheNextBatch)
{
    ASSERT_TRUE(configure("nvidia", { "/usr/bin/nvidia-ctk" }));
    const auto runtimeDir = directory.path() / "runtime";
    ASSERT_TRUE(linglong::cdi_drv::requestRefresh(runtimeDir, ""));
    ASSERT_TRUE(writeFile(systemctl, R"sh(#!/bin/sh
if [ ! -e "${0%/*}/requested" ]; then
    : > "${0%/*}/requested"
    : > "${0%/*}/runtime/pending/refresh-all"
fi
printf '%s\n' "$*" >> "${0%/*}/calls"
)sh"));
    linglong::cdi_drv::RefreshOptions options{ configDir, runtimeDir, {}, true };
    options.idleTimeout = std::chrono::seconds::zero();
    options.watches = linglong::cdi_drv::WatchOptions{ unitDir, systemctl.string() };
    auto result = linglong::cdi_drv::refresh(options);
    ASSERT_TRUE(result) << result.error().message();
    EXPECT_FALSE(fs::exists(runtimeDir / "pending/refresh-all"));
    const auto commands = readFile(calls);
    ASSERT_TRUE(commands);
    const std::string batch = "daemon-reload\n"
                              "--no-block restart -- ll-cdi-drv-watch@nvidia.path\n";
    EXPECT_EQ(*commands, batch + batch);
    EXPECT_TRUE(fs::exists(unitDir / "ll-cdi-drv-watch@nvidia.path"));
    // The missing generator does not prevent installing watches.
    EXPECT_FALSE(fs::exists(runtimeDir / "current"));
}

TEST_F(CdiDrvWatchTest, PublishesGeneratedArtifactsWhenWatchSynchronizationFails)
{
    const json config{ { "version", "1" },
                       { "generator", "linglong" },
                       { "kind", "example.com/test" },
                       { "profile", { { "env", { "TEST=value" } } } },
                       { "watchPathChanged", { "/usr/lib/test.so" } } };
    ASSERT_TRUE(writeFile(configDir / "builtin.json", config.dump()));
    ASSERT_TRUE(writeFile(directory.path() / "fail", ""));
    const auto runtimeDir = directory.path() / "runtime";
    ASSERT_TRUE(linglong::cdi_drv::requestRefresh(runtimeDir, ""));
    linglong::cdi_drv::RefreshOptions options{ configDir, runtimeDir, {}, true };
    options.idleTimeout = std::chrono::seconds::zero();
    options.watches = linglong::cdi_drv::WatchOptions{ unitDir, systemctl.string() };
    auto result = linglong::cdi_drv::refresh(options);
    ASSERT_TRUE(result) << result.error().message();
    EXPECT_TRUE(fs::exists(runtimeDir / "spec/builtin.json"));
    EXPECT_FALSE(fs::exists(runtimeDir / "pending/refresh-all"));
    EXPECT_TRUE(fs::exists(unitDir / "ll-cdi-drv-watch@builtin.path"));
}

TEST_F(CdiDrvWatchTest, SourceChangesDuringWatchCommitApplyOnTheNextRefresh)
{
    const json original{ { "version", "1" },
                         { "generator", "linglong" },
                         { "kind", "example.com/test" },
                         { "profile", { { "env", { "ORIGINAL=1" } } } },
                         { "watchPathChanged", { "/usr/lib/original.so" } } };
    auto updated = original;
    updated["profile"]["env"] = { "UPDATED=1" };
    updated["watchPathChanged"] = { "/usr/lib/updated.so" };
    const auto source = configDir / "builtin.json";
    ASSERT_TRUE(writeFile(source, original.dump()));
    ASSERT_TRUE(writeFile(directory.path() / "updated.json", updated.dump()));
    // Change the source when the generated watch definitions are committed.
    ASSERT_TRUE(writeFile(systemctl, R"sh(#!/bin/sh
/bin/cp "${0%/*}/updated.json" "${0%/*}/config/builtin.json"
)sh"));
    const auto runtimeDir = directory.path() / "runtime";
    linglong::cdi_drv::RefreshOptions options{ configDir, runtimeDir };
    options.watches = { unitDir, systemctl.string() };
    const auto specPath = runtimeDir / "spec/builtin.json";
    const auto watchPath = unitDir / "ll-cdi-drv-watch@builtin.path";
    auto result = linglong::cdi_drv::refresh(options);
    ASSERT_TRUE(result) << result.error().message();
    auto content = readFile(source);
    ASSERT_TRUE(content);
    EXPECT_EQ(json::parse(*content), updated);
    auto spec = readFile(specPath);
    ASSERT_TRUE(spec);
    EXPECT_EQ(json::parse(*spec)["devices"][0]["containerEdits"]["env"],
              json::array({ "ORIGINAL=1" }));
    auto watch = readFile(watchPath);
    ASSERT_TRUE(watch);
    EXPECT_THAT(*watch, HasSubstr("PathChanged=/usr/lib/original.so\n"));

    // The next refresh loads the updated source for both outputs.
    result = linglong::cdi_drv::refresh(options);
    ASSERT_TRUE(result) << result.error().message();
    spec = readFile(specPath);
    ASSERT_TRUE(spec);
    EXPECT_EQ(json::parse(*spec)["devices"][0]["containerEdits"]["env"],
              json::array({ "UPDATED=1" }));
    watch = readFile(watchPath);
    ASSERT_TRUE(watch);
    EXPECT_THAT(*watch, HasSubstr("PathChanged=/usr/lib/updated.so\n"));
}

TEST_F(CdiDrvWatchTest, ParseFailureRemovesTheNamedWatchAndGeneratedArtifacts)
{
    const json config{ { "version", "1" },
                       { "generator", "linglong" },
                       { "kind", "example.com/test" },
                       { "profile", { { "env", { "TEST=value" } } } },
                       { "watchPathChanged", { "/usr/lib/test.so" } } };
    const auto source = configDir / "builtin.json";
    ASSERT_TRUE(writeFile(source, config.dump()));
    const auto runtimeDir = directory.path() / "runtime";
    linglong::cdi_drv::RefreshOptions options{ configDir, runtimeDir, "builtin" };
    options.watches = { unitDir, systemctl.string() };
    const auto watch = unitDir / "ll-cdi-drv-watch@builtin.path";
    for (const auto &invalid : std::vector<std::string>{
           "not json",
           R"({"version":"1","generator":"linglong","profile":{"env":42}})" }) {
        ASSERT_TRUE(writeFile(source, config.dump()));
        auto result = linglong::cdi_drv::refresh(options);
        ASSERT_TRUE(result) << result.error().message();
        EXPECT_TRUE(fs::exists(watch));
        EXPECT_TRUE(fs::exists(runtimeDir / "spec/builtin.json"));
        ASSERT_TRUE(writeFile(source, invalid));
        result = linglong::cdi_drv::refresh(options);
        ASSERT_TRUE(result) << result.error().message();
        EXPECT_FALSE(fs::exists(watch));
        EXPECT_FALSE(fs::exists(runtimeDir / "spec/builtin.json"));
        EXPECT_FALSE(fs::exists(runtimeDir / "cfg/builtin.json"));
    }
}

TEST_F(CdiDrvWatchTest, FullRefreshRemovesDeletedWatchesWithoutPriorGeneratedArtifacts)
{
    ASSERT_TRUE(configure("nvidia", { "/usr/bin/nvidia-ctk" }));
    const auto runtimeDir = directory.path() / "runtime";
    linglong::cdi_drv::RefreshOptions options{ configDir, runtimeDir };
    options.watches = { unitDir, systemctl.string() };
    auto result = linglong::cdi_drv::refresh(options);
    ASSERT_TRUE(result) << result.error().message();
    const auto watch = unitDir / "ll-cdi-drv-watch@nvidia.path";
    EXPECT_TRUE(fs::exists(watch));
    EXPECT_FALSE(fs::exists(runtimeDir / "current"));
    ASSERT_TRUE(fs::remove(configDir / "nvidia.json"));
    result = linglong::cdi_drv::refresh(options);
    ASSERT_TRUE(result) << result.error().message();
    EXPECT_FALSE(fs::exists(watch));
}

TEST_F(CdiDrvWatchTest, ProcessesConfigurationsIndividuallyAndReloadsWatchesOnceAtTheEnd)
{
    const auto generator = directory.path() / "generate-a";
    ASSERT_TRUE(writeFile(generator, R"sh(#!/bin/sh
if [ -e "${0%/*}/calls" ]; then
    exit 1
fi
/bin/cp "${0%/*}/updated-a.json" "${0%/*}/config/a.json"
/bin/cp "${0%/*}/updated-b.json" "${0%/*}/config/b.json"
printf '%s' "$1"
)sh"));
    fs::permissions(generator, fs::perms::owner_all);
    const std::string spec =
      R"({"cdiVersion":"0.6.0","kind":"example.com/test","devices":[{"name":"a","containerEdits":{"env":["A=1"]}}]})";
    const json configA{ { "version", "1" },
                        { "generator", "command" },
                        { "argv", { generator.string(), spec } },
                        { "watchPathChanged", { "/usr/lib/a-original.so" } } };
    const json configB{ { "version", "1" },
                        { "generator", "linglong" },
                        { "kind", "example.com/test" },
                        { "profile", { { "env", { "BEFORE=1" } } } },
                        { "watchPathChanged", { "/usr/lib/b-original.so" } } };
    auto updatedA = configA;
    updatedA["watchPathChanged"] = { "/usr/lib/a-updated.so" };
    auto updatedB = configB;
    updatedB["profile"]["env"] = { "AFTER=1" };
    updatedB["watchPathChanged"] = { "/usr/lib/b-updated.so" };
    ASSERT_TRUE(writeFile(configDir / "a.json", configA.dump()));
    ASSERT_TRUE(writeFile(configDir / "b.json", configB.dump()));
    ASSERT_TRUE(writeFile(directory.path() / "updated-a.json", updatedA.dump()));
    ASSERT_TRUE(writeFile(directory.path() / "updated-b.json", updatedB.dump()));
    const auto runtimeDir = directory.path() / "runtime";
    linglong::cdi_drv::RefreshOptions options{ configDir, runtimeDir };
    options.watches = { unitDir, systemctl.string() };
    auto result = linglong::cdi_drv::refresh(options);
    ASSERT_TRUE(result) << result.error().message();
    EXPECT_TRUE(fs::exists(runtimeDir / "spec/a.json"));
    const auto generatedB = readFile(runtimeDir / "spec/b.json");
    ASSERT_TRUE(generatedB);
    EXPECT_EQ(json::parse(*generatedB)["devices"][0]["containerEdits"]["env"],
              json::array({ "AFTER=1" }));
    const auto watchA = readFile(unitDir / "ll-cdi-drv-watch@a.path");
    ASSERT_TRUE(watchA);
    EXPECT_THAT(*watchA, HasSubstr("PathChanged=/usr/lib/a-original.so\n"));
    const auto watchB = readFile(unitDir / "ll-cdi-drv-watch@b.path");
    ASSERT_TRUE(watchB);
    EXPECT_THAT(*watchB, HasSubstr("PathChanged=/usr/lib/b-updated.so\n"));
    const auto commands = readFile(calls);
    ASSERT_TRUE(commands);
    EXPECT_EQ(*commands,
              "daemon-reload\n"
              "--no-block restart -- ll-cdi-drv-watch@a.path ll-cdi-drv-watch@b.path\n");
}

TEST_F(CdiDrvWatchTest, CommitsAllPreparedWatchesOnceWhenEveryGeneratorFails)
{
    ASSERT_TRUE(configure("a", { "/usr/lib/a.so" }));
    ASSERT_TRUE(configure("b", { "/usr/lib/b.so" }));
    const auto runtimeDir = directory.path() / "runtime";
    linglong::cdi_drv::RefreshOptions options{ configDir, runtimeDir };
    options.watches = { unitDir, systemctl.string() };
    auto result = linglong::cdi_drv::refresh(options);
    ASSERT_TRUE(result) << result.error().message();
    EXPECT_FALSE(fs::exists(runtimeDir / "current"));
    EXPECT_TRUE(fs::exists(unitDir / "ll-cdi-drv-watch@a.path"));
    EXPECT_TRUE(fs::exists(unitDir / "ll-cdi-drv-watch@b.path"));
    const auto commands = readFile(calls);
    ASSERT_TRUE(commands);
    EXPECT_EQ(*commands,
              "daemon-reload\n"
              "--no-block restart -- ll-cdi-drv-watch@a.path ll-cdi-drv-watch@b.path\n");
}

TEST_F(CdiDrvWatchTest, ReactivatesUnchangedWatchAfterRestartFailureOnNextRefresh)
{
    ASSERT_TRUE(configure("nvidia", { "/usr/bin/nvidia-ctk" }));
    ASSERT_TRUE(sync());
    const auto failure = directory.path() / "fail-restart";
    ASSERT_TRUE(writeFile(failure, ""));
    EXPECT_FALSE(sync({ false, { "nvidia" } }));
    ASSERT_TRUE(fs::remove(failure));
    ASSERT_TRUE(writeFile(calls, ""));
    ASSERT_TRUE(sync({ false, { "nvidia" } }));
    const auto commands = readFile(calls);
    ASSERT_TRUE(commands);
    EXPECT_EQ(*commands,
              "daemon-reload\n"
              "--no-block restart -- ll-cdi-drv-watch@nvidia.path\n");
    EXPECT_FALSE(fs::exists(unitDir / ".ll-cdi-drv-watch-dirty"));
}

} // namespace
