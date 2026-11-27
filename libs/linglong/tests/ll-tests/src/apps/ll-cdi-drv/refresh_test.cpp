// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "common/tempdir.h"
#include "common/test_process.h"
#include "linglong/cdi/cdi.h"
#include "linglong/cli/cli.h"
#include "linglong/runtime/cdi_config.h"
#include "linglong/runtime/container_builder.h"
#include "linglong/utils/file.h"

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <signal.h>
#include <sys/stat.h>

namespace {

namespace fs = std::filesystem;
using linglong::utils::readFile;
using linglong::utils::writeFile;
using nlohmann::json;
using testing::HasSubstr;
using testing::Not;

struct CliResult
{
    int status;
    std::string output;
    std::string error;
};

CliResult runCli(const std::vector<std::string> &arguments,
                 const std::map<std::string, std::string> &environment = {})
{
    linglong::test::TestProcess child(LL_CDI_DRV_BINARY, arguments, environment);
    const auto status = child.wait();
    if (status < 0) {
        ADD_FAILURE() << "ll-cdi-drv timed out or exited abnormally";
    }
    return { status, child.stdoutOutput(), child.stderrOutput() };
}

testing::AssertionResult succeeded(const CliResult &result)
{
    if (result.status != 0) {
        return testing::AssertionFailure() << "ll-cdi-drv exited with " << result.status << '\n'
                                           << result.output << result.error;
    }
    return testing::AssertionSuccess();
}

testing::AssertionResult succeeded(const linglong::utils::error::Result<void> &result)
{
    if (!result) {
        return testing::AssertionFailure() << result.error().message();
    }
    return testing::AssertionSuccess();
}

bool entryExists(const fs::path &path)
{
    return fs::exists(fs::symlink_status(path));
}

class CdiDrvRefreshTest : public testing::Test
{
protected:
    void SetUp() override
    {
        ASSERT_TRUE(fs::create_directory(configDir));
        ASSERT_TRUE(succeeded(writeFile(systemctl, "#!/bin/sh\nexit 0\n")));
        fs::permissions(systemctl, fs::perms::owner_all);
    }

    CliResult run(const std::string &subcommand, std::vector<std::string> arguments = {}) const
    {
        arguments.insert(arguments.begin(),
                         { subcommand,
                           "--runtime-dir",
                           runtimeDir.string(),
                           "--unit-dir",
                           unitDir.string(),
                           "--systemctl",
                           systemctl.string() });
        return runCli(arguments);
    }

    CliResult refresh(std::vector<std::string> arguments = {}) const
    {
        arguments.insert(arguments.begin(), { "--config-dir", configDir.string() });
        return run("refresh", std::move(arguments));
    }

    std::vector<std::string> pendingArguments(unsigned int idleTimeout = 0) const
    {
        return { "refresh-pending",  "--runtime-dir",  runtimeDir.string(),        "--config-dir",
                 configDir.string(), "--unit-dir",     unitDir.string(),           "--systemctl",
                 systemctl.string(), "--idle-timeout", std::to_string(idleTimeout) };
    }

    CliResult refreshPending() const { return runCli(pendingArguments()); }

    testing::AssertionResult writeBuiltin(bool requiresMissingNode = false) const
    {
        auto config = json::parse(R"json({
            "version": "1",
            "generator": "linglong",
            "kind": "example.com/test",
            "defaultDevice": "render",
            "profile": {
                "deviceNodes": [{"path": "/dev/test", "hostPath": "/dev/null"}],
                "env": ["GLOBAL=present", "MODE=base"]
            },
            "deviceProfile": [{
                "device": "render",
                "profile": {
                    "deviceNodes": [{"path": "/dev/extra-test", "hostPath": "/dev/zero"}],
                    "env": ["MODE=render"]
                }
            }]
        })json");
        const auto mountSource = directory.path() / "mount-source";
        auto mount = writeFile(mountSource, "driver data");
        if (!mount) {
            return succeeded(mount);
        }
        config["profile"]["mountFiles"] = { mountSource.string() };
        config["deviceProfile"][0]["profile"]["deviceNodes"].push_back(
          { { "path", (directory.path() / "missing").string() },
            { "required", requiresMissingNode } });
        return succeeded(writeFile(configDir / "builtin.json", config.dump()));
    }

    testing::AssertionResult writeCommand(const std::string &device = "all",
                                          const std::string &version = "0.6.0") const
    {
        const auto spec = fmt::format(R"yaml(cdiVersion: {}
kind: other.example.com/test
devices:
  - name: {}
    containerEdits:
      env: [COMMAND_ENV=value]
)yaml",
                                      json(version).dump(),
                                      json(device).dump());
        const json config{ { "version", "1" },
                           { "generator", "command" },
                           { "argv", json::array({ "/usr/bin/printf", "%s", spec }) },
                           { "defaultDevice", device } };
        return succeeded(writeFile(configDir / "command.json", config.dump()));
    }

    std::set<fs::path> generations() const
    {
        std::set<fs::path> result;
        for (const auto &entry : fs::directory_iterator(runtimeDir)) {
            if (entry.path().filename().string().compare(0, 11, "generation-") == 0) {
                result.insert(entry.path());
            }
        }
        return result;
    }

    TempDir directory;
    const fs::path configDir{ directory.path() / "config" };
    const fs::path runtimeDir{ directory.path() / "runtime" };
    const fs::path unitDir{ directory.path() / "units" };
    const fs::path systemctl{ directory.path() / "systemctl" };
    const fs::path current{ runtimeDir / "current" };
    const fs::path specDir{ runtimeDir / "spec" };
    const fs::path cfgDir{ runtimeDir / "cfg" };
    const fs::path fullRequest{ runtimeDir / "pending/refresh-all" };
    const fs::path singleRequests{ runtimeDir / "pending/refresh-single" };
};

TEST_F(CdiDrvRefreshTest, RejectsInvalidArguments)
{
    const auto appHelp = runCli({ "--help" });
    ASSERT_TRUE(succeeded(appHelp));
    EXPECT_THAT(appHelp.output, Not(HasSubstr("sync-watches")));
    const auto pendingHelp = run("refresh-pending", { "--help" });
    ASSERT_TRUE(succeeded(pendingHelp));
    EXPECT_THAT(pendingHelp.output, Not(HasSubstr("--sync-watches")));
    EXPECT_THAT(pendingHelp.output, HasSubstr("--idle-timeout"));
    const auto negativeTimeout = run("refresh-pending", { "--idle-timeout", "-1" });
    EXPECT_GT(negativeTimeout.status, 0);
    EXPECT_THAT(negativeTimeout.error, HasSubstr("--idle-timeout"));
    const auto help = run("refresh", { "--help" });
    ASSERT_TRUE(succeeded(help));
    EXPECT_THAT(help.output, HasSubstr("omit to refresh all"));

    for (const auto *name :
         { "../nvidia", ".hidden", "gpu name", "gpu${FOO}", "gpu%", "gpu@1", "gpu:1", "显卡" }) {
        for (auto arguments :
             { std::vector<std::string>{}, std::vector<std::string>{ "--request" } }) {
            arguments.emplace_back(name);
            const auto rejected = run("refresh", std::move(arguments));
            EXPECT_GT(rejected.status, 0);
            EXPECT_THAT(rejected.error, HasSubstr("Expected a configuration name"));
        }
    }

    const auto conflicting = run("refresh", { "--request-all", "--request", "nvidia" });
    EXPECT_GT(conflicting.status, 0);
    EXPECT_THAT(conflicting.error, HasSubstr("excludes"));
}

TEST_F(CdiDrvRefreshTest, RejectsWritableRuntimeDirectory)
{
    ASSERT_TRUE(fs::create_directory(runtimeDir));
    for (const auto permission : { fs::perms::group_write, fs::perms::others_write }) {
        fs::permissions(runtimeDir, fs::perms::owner_all | permission);
        for (const auto &arguments :
             { std::vector<std::string>{}, std::vector<std::string>{ "--request-all" } }) {
            const auto rejected = arguments.empty() ? refresh() : run("refresh", arguments);
            EXPECT_GT(rejected.status, 0);
            EXPECT_THAT(rejected.error,
                        HasSubstr("expected a directory writable only by its owner"));
        }
    }
    EXPECT_FALSE(entryExists(runtimeDir / "refresh.lock"));
    EXPECT_FALSE(entryExists(runtimeDir / "pending"));
}

TEST_F(CdiDrvRefreshTest, RejectsSymlinkRuntimeDirectory)
{
    const auto target = directory.path() / "runtime-target";
    ASSERT_TRUE(fs::create_directory(target));
    fs::create_directory_symlink(target, runtimeDir);
    for (const auto &arguments :
         { std::vector<std::string>{}, std::vector<std::string>{ "--request-all" } }) {
        const auto rejected = arguments.empty() ? refresh() : run("refresh", arguments);
        EXPECT_GT(rejected.status, 0);
        EXPECT_THAT(rejected.error, HasSubstr("expected a directory writable only by its owner"));
    }
    EXPECT_FALSE(entryExists(target / "refresh.lock"));
    EXPECT_FALSE(entryExists(target / "pending"));
}

TEST_F(CdiDrvRefreshTest, QueuesNamedRequestsInNewRuntimeDirectory)
{
    for (const auto *name : { "nvidia", "all", "GPU-0.1_test" }) {
        for (int attempt = 0; attempt < 2; ++attempt) {
            ASSERT_TRUE(succeeded(run("refresh", { "--request", name })));
            EXPECT_TRUE(fs::is_regular_file(singleRequests / name));
        }
    }

    EXPECT_FALSE(entryExists(fullRequest));
    EXPECT_FALSE(entryExists(current));
    EXPECT_FALSE(entryExists(runtimeDir / "refresh.lock"));
    const auto permissions = fs::perms::owner_all | fs::perms::group_read | fs::perms::group_exec
      | fs::perms::others_read | fs::perms::others_exec;
    for (const auto &path : { runtimeDir, runtimeDir / "pending", singleRequests }) {
        EXPECT_EQ(fs::status(path).permissions() & fs::perms::all, permissions);
    }
}

TEST_F(CdiDrvRefreshTest, QueuesFullRequestInNewRuntimeDirectory)
{
    for (int attempt = 0; attempt < 2; ++attempt) {
        ASSERT_TRUE(succeeded(run("refresh", { "--request-all" })));
        EXPECT_TRUE(fs::is_regular_file(fullRequest));
    }

    EXPECT_FALSE(entryExists(singleRequests));
    EXPECT_FALSE(entryExists(current));
    EXPECT_FALSE(entryExists(runtimeDir / "refresh.lock"));
}

TEST_F(CdiDrvRefreshTest, ConsumesNewRequestsWhileFullRefreshIsGenerating)
{
    ASSERT_TRUE(writeCommand());
    const auto source = configDir / "command.json";
    const auto content = readFile(source);
    ASSERT_TRUE(content);
    auto config = json::parse(*content);
    const auto spec = config["argv"][2].get<std::string>();
    const auto ready = directory.path() / "generator-ready";
    const auto release = directory.path() / "generator-release";
    config["argv"] = json::array({
      "/bin/sh",
      "-c",
      R"sh(printf 'run\n' >> "$1"; while [ ! -e "$2" ]; do sleep 0.01; done; printf '%s' "$3")sh",
      "cdi-test-generator",
      ready.string(),
      release.string(),
      spec,
    });
    ASSERT_TRUE(succeeded(writeFile(source, config.dump())));
    ASSERT_TRUE(succeeded(run("refresh", { "--request-all" })));
    ASSERT_TRUE(succeeded(run("refresh", { "--request", "command" })));

    linglong::test::TestProcess worker(LL_CDI_DRV_BINARY, pendingArguments());
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!fs::exists(ready) && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    ASSERT_TRUE(fs::exists(ready)) << worker.stderrOutput();
    EXPECT_FALSE(entryExists(fullRequest));
    EXPECT_FALSE(entryExists(singleRequests));

    linglong::test::TestProcess writer(
      LL_CDI_DRV_BINARY,
      { "refresh", "--runtime-dir", runtimeDir.string(), "--request", "command" });
    EXPECT_EQ(writer.wait(std::chrono::seconds(5)), 0) << writer.stderrOutput();
    ASSERT_TRUE(succeeded(writeFile(release, "release")));
    EXPECT_EQ(worker.wait(), 0) << worker.stderrOutput();
    EXPECT_EQ(writer.wait(), 0) << writer.stderrOutput();
    EXPECT_TRUE(fs::is_regular_file(specDir / "command.yaml"));
    EXPECT_FALSE(entryExists(singleRequests / "command"));
    const auto runs = readFile(ready);
    ASSERT_TRUE(runs);
    EXPECT_EQ(*runs, "run\nrun\n");
}

TEST_F(CdiDrvRefreshTest, ConsumesRequestsWhileIdleAndResetsIdleTimeout)
{
    ASSERT_TRUE(writeBuiltin());
    ASSERT_TRUE(succeeded(run("refresh", { "--request-all" })));
    linglong::test::TestProcess worker(LL_CDI_DRV_BINARY, pendingArguments(2));
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!fs::exists(specDir / "builtin.json") && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    ASSERT_TRUE(fs::exists(specDir / "builtin.json")) << worker.stderrOutput();

    // An idle consumer must release refresh.lock so a manual refresh can complete.
    linglong::test::TestProcess manual(LL_CDI_DRV_BINARY,
                                       { "refresh",
                                         "builtin",
                                         "--runtime-dir",
                                         runtimeDir.string(),
                                         "--config-dir",
                                         configDir.string(),
                                         "--unit-dir",
                                         unitDir.string(),
                                         "--systemctl",
                                         systemctl.string() });
    ASSERT_EQ(manual.wait(std::chrono::seconds(2)), 0) << manual.stderrOutput();

    std::this_thread::sleep_for(std::chrono::milliseconds(600));
    ASSERT_TRUE(writeCommand());
    const auto requestedAt = std::chrono::steady_clock::now();
    ASSERT_TRUE(succeeded(run("refresh", { "--request", "command" })));
    const auto publishedBy = requestedAt + std::chrono::seconds(2);
    while (!fs::exists(specDir / "command.yaml")
           && std::chrono::steady_clock::now() < publishedBy) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    ASSERT_TRUE(fs::exists(specDir / "command.yaml")) << worker.stderrOutput();
    EXPECT_FALSE(entryExists(singleRequests / "command"));
    EXPECT_EQ(worker.wait(), 0) << worker.stderrOutput();
    EXPECT_GE(std::chrono::steady_clock::now() - requestedAt, std::chrono::seconds(2));
}

TEST_F(CdiDrvRefreshTest, ConsumesQueuedRequestsWithoutIdleWait)
{
    ASSERT_TRUE(writeBuiltin());
    ASSERT_TRUE(succeeded(run("refresh", { "--request-all" })));
    const auto startedAt = std::chrono::steady_clock::now();
    ASSERT_TRUE(succeeded(refreshPending()));
    EXPECT_LT(std::chrono::steady_clock::now() - startedAt, std::chrono::seconds(2));
    EXPECT_TRUE(fs::exists(specDir / "builtin.json"));
    EXPECT_FALSE(entryExists(fullRequest));
}

TEST_F(CdiDrvRefreshTest, SkipsInvalidQueuedNamesAndProcessesValidRequests)
{
    ASSERT_TRUE(writeBuiltin());
    ASSERT_TRUE(writeCommand());
    ASSERT_TRUE(succeeded(run("refresh", { "--request", "builtin" })));
    ASSERT_TRUE(succeeded(run("refresh", { "--request", "command" })));
    const std::vector<std::string> invalidNames{ "legacy$gpu", "gpu name", "gpu:1", "显卡" };
    for (const auto &name : invalidNames) {
        fs::copy_file(configDir / "builtin.json", configDir / (name + ".json"));
        ASSERT_TRUE(succeeded(writeFile(singleRequests / name, "")));
    }

    const auto refreshed = refreshPending();
    ASSERT_TRUE(succeeded(refreshed));
    EXPECT_TRUE(fs::is_empty(singleRequests));
    EXPECT_TRUE(fs::is_regular_file(specDir / "builtin.json"));
    EXPECT_TRUE(fs::is_regular_file(specDir / "command.yaml"));
    EXPECT_TRUE(fs::is_regular_file(cfgDir / "builtin.json"));
    EXPECT_TRUE(fs::is_regular_file(cfgDir / "command.json"));
    for (const auto &name : invalidNames) {
        EXPECT_THAT(refreshed.error, HasSubstr("Skipping CDI refresh request " + name));
        EXPECT_FALSE(entryExists(specDir / (name + ".json")));
        EXPECT_FALSE(entryExists(cfgDir / (name + ".json")));
    }
}

TEST_F(CdiDrvRefreshTest, DrainsInvalidOnlyRequestBatchWithoutPublishing)
{
    ASSERT_TRUE(fs::create_directories(singleRequests));
    ASSERT_TRUE(succeeded(writeFile(singleRequests / "legacy$gpu", "")));
    ASSERT_TRUE(succeeded(writeFile(singleRequests / "gpu name", "")));

    const auto refreshed = refreshPending();
    ASSERT_TRUE(succeeded(refreshed));
    EXPECT_THAT(refreshed.error, HasSubstr("Skipping CDI refresh request legacy$gpu"));
    EXPECT_THAT(refreshed.error, HasSubstr("Skipping CDI refresh request gpu name"));
    EXPECT_TRUE(fs::is_empty(singleRequests));
    EXPECT_FALSE(entryExists(current));
    EXPECT_FALSE(entryExists(specDir));
    EXPECT_FALSE(entryExists(cfgDir));
}

TEST_F(CdiDrvRefreshTest, SkipsPublicationForEmptyConfigDirectory)
{
    const auto startedAt = std::chrono::steady_clock::now();
    ASSERT_TRUE(succeeded(run("refresh-pending", { "--config-dir", configDir.string() })));
    EXPECT_GE(std::chrono::steady_clock::now() - startedAt, std::chrono::seconds(5));
    EXPECT_FALSE(entryExists(runtimeDir / "pending"));
    ASSERT_TRUE(succeeded(refresh()));
    ASSERT_TRUE(succeeded(run("refresh", { "--request-all" })));
    ASSERT_TRUE(succeeded(refreshPending()));

    EXPECT_TRUE(generations().empty());
    EXPECT_FALSE(entryExists(current));
    EXPECT_FALSE(entryExists(specDir));
    EXPECT_FALSE(entryExists(cfgDir));
    EXPECT_FALSE(entryExists(fullRequest));
}

TEST_F(CdiDrvRefreshTest, IgnoresHiddenConfigurationAndRequestFiles)
{
    ASSERT_TRUE(writeBuiltin());
    ASSERT_TRUE(succeeded(writeFile(configDir / ".hidden.json", "invalid JSON")));
    ASSERT_TRUE(succeeded(run("refresh", { "--request", "builtin" })));
    const auto hiddenRequest = singleRequests / ".hidden";
    ASSERT_TRUE(succeeded(writeFile(hiddenRequest, "")));

    const auto all = refresh();
    ASSERT_TRUE(succeeded(all));
    EXPECT_THAT(all.error, Not(HasSubstr(".hidden")));
    EXPECT_TRUE(fs::is_regular_file(specDir / "builtin.json"));
    EXPECT_FALSE(entryExists(specDir / ".hidden.json"));

    const auto pending = refreshPending();
    ASSERT_TRUE(succeeded(pending));
    EXPECT_THAT(pending.error, Not(HasSubstr(".hidden")));
    EXPECT_TRUE(fs::is_regular_file(hiddenRequest));
    EXPECT_FALSE(entryExists(singleRequests / "builtin"));
}

TEST_F(CdiDrvRefreshTest, PublishesBuiltinAndCommandSpecs)
{
    ASSERT_TRUE(writeBuiltin());
    ASSERT_TRUE(writeCommand());
    ASSERT_TRUE(succeeded(refresh()));

    const auto builtinConfig = readFile(cfgDir / "builtin.json");
    ASSERT_TRUE(builtinConfig);
    EXPECT_THAT(*builtinConfig, HasSubstr("example.com/test=render"));
    EXPECT_EQ(json::parse(*builtinConfig)["devices"][0]["spec"], "../spec/builtin.json");
    const auto commandConfig = readFile(cfgDir / "command.json");
    ASSERT_TRUE(commandConfig);
    EXPECT_THAT(*commandConfig, HasSubstr("other.example.com/test=all"));
    EXPECT_EQ(json::parse(*commandConfig)["devices"][0]["spec"], "../spec/command.yaml");

    const auto builtinSpec = readFile(specDir / "builtin.json");
    ASSERT_TRUE(builtinSpec);
    EXPECT_THAT(*builtinSpec, HasSubstr("\"hostPath\":\"/dev/zero\""));
    EXPECT_THAT(*builtinSpec, HasSubstr("GLOBAL=present"));
    EXPECT_THAT(*builtinSpec, HasSubstr("mount-source"));
    const auto commandSpec = readFile(specDir / "command.yaml");
    ASSERT_TRUE(commandSpec);
    EXPECT_THAT(*commandSpec, HasSubstr("COMMAND_ENV=value"));

    ASSERT_TRUE(fs::is_symlink(current));
    const auto generation = fs::read_symlink(current);
    const auto published = runtimeDir / generation;
    EXPECT_FALSE(entryExists(published / "sources.json"));
    const auto generated = linglong::runtime::loadCdiDriverGeneration(runtimeDir);
    EXPECT_EQ(generated.specDir, fs::canonical(published / "spec"));
    linglong::cli::RunOptions runOptions;
    runOptions.cdiSpecDir.clear();
    auto result = linglong::runtime::resolveCdiDevices(generated, std::nullopt, runOptions);
    ASSERT_TRUE(result) << result.error().message();
    ASSERT_TRUE(*result);
    ASSERT_EQ((*result)->size(), 2);
    EXPECT_EQ(fs::path((*result)->at(0).spec.path).lexically_normal(),
              published / "spec/builtin.json");
    EXPECT_EQ(fs::path((*result)->at(1).spec.path).lexically_normal(),
              published / "spec/command.yaml");

    ASSERT_TRUE(succeeded(refresh()));
    EXPECT_EQ(fs::read_symlink(current), generation);
}

TEST_F(CdiDrvRefreshTest, PublishesDuplicateDeviceNamesWhenOnlyOneIsAutoEnabled)
{
    ASSERT_TRUE(writeCommand());
    const auto content = readFile(configDir / "command.json");
    ASSERT_TRUE(content);
    auto other = json::parse(*content);
    other["autoEnable"] = false;
    ASSERT_TRUE(succeeded(writeFile(configDir / "other.json", other.dump())));
    ASSERT_TRUE(succeeded(refresh()));

    EXPECT_TRUE(fs::is_regular_file(specDir / "command.yaml"));
    EXPECT_TRUE(fs::is_regular_file(specDir / "other.yaml"));
    const auto cfg = readFile(cfgDir / "other.json");
    ASSERT_TRUE(cfg);
    EXPECT_TRUE(json::parse(*cfg).at("devices").empty());
    linglong::cli::RunOptions options;
    options.cdiSpecDir.clear();
    const auto generation = linglong::runtime::loadCdiDriverGeneration(runtimeDir);
    auto devices = linglong::runtime::resolveCdiDevices(generation, std::nullopt, options);
    ASSERT_TRUE(devices) << devices.error().message();
    ASSERT_TRUE(*devices);
    ASSERT_EQ((**devices).size(), 1);
    EXPECT_EQ(fs::path((**devices).front().spec.path).filename(), "command.yaml");

    options.cdiDevices = { linglong::api::types::v1::CdiDeviceRequest{
      "other.example.com/test=all",
      (specDir / "other.yaml").string() } };
    devices = linglong::runtime::resolveCdiDevices(generation, std::nullopt, options);
    ASSERT_TRUE(devices) << devices.error().message();
    ASSERT_TRUE(*devices);
    ASSERT_EQ((**devices).size(), 1);
    EXPECT_EQ(fs::path((**devices).front().spec.path).filename(), "other.yaml");
}

TEST_F(CdiDrvRefreshTest, PublishesAllSpecsButSelectsFirstAutoEnabledNameOnFullAndNamedRefresh)
{
    ASSERT_TRUE(writeCommand());
    ASSERT_TRUE(succeeded(refresh()));
    auto previous = fs::read_symlink(current);
    const auto content = readFile(configDir / "command.json");
    ASSERT_TRUE(content);
    auto other = json::parse(*content);
    const auto spec = other["argv"][2].get<std::string>();

    for (int mode = 0; mode < 3; ++mode) {
        SCOPED_TRACE(mode);
        auto otherSpec = spec;
        const std::string originalEnv = "COMMAND_ENV=value";
        const auto envPosition = otherSpec.find(originalEnv);
        ASSERT_NE(envPosition, std::string::npos);
        otherSpec.replace(envPosition, originalEnv.size(), "COMMAND_ENV=other");
        other["argv"][2] = otherSpec + "\n# refresh " + std::to_string(mode) + "\n";
        ASSERT_TRUE(succeeded(writeFile(configDir / "other.json", other.dump())));
        if (mode == 2) {
            ASSERT_TRUE(succeeded(run("refresh", { "--request", "other" })));
        }
        const auto refreshed = mode == 0 ? refresh()
          : mode == 1                    ? refresh({ "other" })
                                         : refreshPending();
        ASSERT_TRUE(succeeded(refreshed));
        EXPECT_NE(fs::read_symlink(current), previous);
        previous = fs::read_symlink(current);
        EXPECT_TRUE(fs::is_regular_file(specDir / "command.yaml"));
        EXPECT_TRUE(fs::is_regular_file(specDir / "other.yaml"));
        EXPECT_TRUE(fs::is_regular_file(cfgDir / "other.json"));

        linglong::cli::RunOptions options;
        options.cdiSpecDir.clear();
        const auto generation = linglong::runtime::loadCdiDriverGeneration(runtimeDir);
        testing::internal::CaptureStderr();
        auto devices = linglong::runtime::resolveCdiDevices(generation, std::nullopt, options);
        const auto diagnostics = testing::internal::GetCapturedStderr();
        ASSERT_TRUE(devices) << devices.error().message();
        ASSERT_TRUE(*devices);
        ASSERT_EQ((**devices).size(), 1);
        EXPECT_EQ(fs::path((**devices).front().spec.path).filename(), "command.yaml");
        const auto &skipped =
          std::get<linglong::api::types::v1::CdiDeviceRequest>(generation.devices.at(1));
        EXPECT_THAT(diagnostics,
                    HasSubstr("Skipping repeated CDI device other.example.com/test=all from "
                              + skipped.spec + "; keeping " + (**devices).front().spec.path));
        const auto edits = linglong::cdi::getCDIDeviceEdits((**devices).front());
        ASSERT_TRUE(edits) << edits.error().message();
        ASSERT_TRUE(edits->env);
        EXPECT_EQ(*edits->env, (std::vector<std::string>{ "COMMAND_ENV=value" }));

        options.cdiDevices = { "other.example.com/test=all" };
        devices = linglong::runtime::resolveCdiDevices(
          linglong::runtime::loadCdiDriverGeneration(runtimeDir),
          std::nullopt,
          options);
        ASSERT_TRUE(devices) << devices.error().message();
        ASSERT_TRUE(*devices);
        ASSERT_EQ((**devices).size(), 1);
        EXPECT_EQ(fs::path((**devices).front().spec.path).filename(), "command.yaml");
    }
}

TEST_F(CdiDrvRefreshTest, PublishesSameAutoEnabledNameBeforeFirstPublication)
{
    ASSERT_TRUE(writeCommand());
    ASSERT_TRUE(fs::copy_file(configDir / "command.json", configDir / "other.json"));
    ASSERT_TRUE(succeeded(refresh()));
    EXPECT_TRUE(entryExists(current));
    EXPECT_EQ(generations().size(), 1);
    linglong::cli::RunOptions options;
    options.cdiSpecDir.clear();
    auto devices =
      linglong::runtime::resolveCdiDevices(linglong::runtime::loadCdiDriverGeneration(runtimeDir),
                                           std::nullopt,
                                           options);
    ASSERT_TRUE(devices) << devices.error().message();
    ASSERT_TRUE(*devices);
    ASSERT_EQ((**devices).size(), 1);
    EXPECT_EQ(fs::path((**devices).front().spec.path).filename(), "command.yaml");
    EXPECT_TRUE(fs::is_regular_file(specDir / "command.yaml"));
    EXPECT_TRUE(fs::is_regular_file(specDir / "other.yaml"));
    EXPECT_TRUE(fs::is_regular_file(cfgDir / "command.json"));
    EXPECT_TRUE(fs::is_regular_file(cfgDir / "other.json"));
}

TEST_F(CdiDrvRefreshTest, AllowsSameShortDeviceNameForDifferentKinds)
{
    ASSERT_TRUE(writeBuiltin());
    ASSERT_TRUE(writeCommand("render"));
    ASSERT_TRUE(succeeded(refresh()));
    linglong::cli::RunOptions options;
    options.cdiSpecDir.clear();
    auto devices =
      linglong::runtime::resolveCdiDevices(linglong::runtime::loadCdiDriverGeneration(runtimeDir),
                                           std::nullopt,
                                           options);
    ASSERT_TRUE(devices) << devices.error().message();
    ASSERT_TRUE(*devices);
    EXPECT_EQ((**devices).size(), 2);
}

TEST_F(CdiDrvRefreshTest, PreservesDottedConfigurationNamesOnNamedRefresh)
{
    ASSERT_TRUE(writeBuiltin());
    fs::rename(configDir / "builtin.json", configDir / "vendor.gpu.json");
    ASSERT_TRUE(writeCommand());
    ASSERT_TRUE(succeeded(refresh()));
    const auto spec = readFile(specDir / "vendor.gpu.json");
    ASSERT_TRUE(spec);

    ASSERT_TRUE(writeCommand("updated"));
    ASSERT_TRUE(succeeded(refresh({ "command" })));
    const auto preserved = readFile(specDir / "vendor.gpu.json");
    ASSERT_TRUE(preserved);
    EXPECT_EQ(*preserved, *spec);
    const auto config = readFile(cfgDir / "vendor.gpu.json");
    ASSERT_TRUE(config);
    EXPECT_EQ(json::parse(*config)["devices"][0]["spec"], "../spec/vendor.gpu.json");
    EXPECT_TRUE(linglong::cdi::getCDIDevicesFromSpec(specDir / "vendor.gpu.json"));
}

TEST_F(CdiDrvRefreshTest, NamedRefreshPreservesUnchangedDeviceContainerId)
{
    ASSERT_TRUE(writeBuiltin());
    ASSERT_TRUE(writeCommand());
    ASSERT_TRUE(succeeded(refresh()));
    linglong::cli::RunOptions options;
    options.cdiSpecDir.clear();
    options.cdiDevices = { "other.example.com/test=all" };
    auto devices =
      linglong::runtime::resolveCdiDevices(linglong::runtime::loadCdiDriverGeneration(runtimeDir),
                                           std::nullopt,
                                           options);
    ASSERT_TRUE(devices) << devices.error().message();
    ASSERT_TRUE(*devices);
    ASSERT_EQ((**devices).size(), 1);
    linglong::api::types::v1::RunContextConfig before;
    before.version = "1";
    before.cdiDevices = **devices;

    ASSERT_TRUE(writeBuiltin(true));
    ASSERT_TRUE(succeeded(refresh({ "builtin" })));
    devices =
      linglong::runtime::resolveCdiDevices(linglong::runtime::loadCdiDriverGeneration(runtimeDir),
                                           std::nullopt,
                                           options);
    ASSERT_TRUE(devices) << devices.error().message();
    ASSERT_TRUE(*devices);
    ASSERT_EQ((**devices).size(), 1);
    auto after = before;
    after.cdiDevices = **devices;
    EXPECT_NE(before.cdiDevices->front().spec.path, after.cdiDevices->front().spec.path);
    EXPECT_EQ(before.cdiDevices->front().spec.checksum, after.cdiDevices->front().spec.checksum);
    EXPECT_EQ(linglong::runtime::genContainerID(before), linglong::runtime::genContainerID(after));
    EXPECT_TRUE(linglong::cdi::getCDIDeviceEdits(before.cdiDevices->front()));
    EXPECT_TRUE(linglong::cdi::getCDIDeviceEdits(after.cdiDevices->front()));
}

TEST_F(CdiDrvRefreshTest, CommandLooksUpExecutableAndUsesOutputSpec)
{
    ASSERT_TRUE(writeCommand());
    const auto source = configDir / "command.json";
    auto content = readFile(source);
    ASSERT_TRUE(content);
    auto config = json::parse(*content);
    config["argv"][0] = "printf";
    config["kind"] = "ignored.example.com/test";
    config["profile"] = { { "env", { "UNUSED_PROFILE=value" } } };
    config["deviceProfile"] =
      json::array({ { { "device", "render" }, { "profile", json::object() } } });
    ASSERT_TRUE(succeeded(writeFile(source, config.dump())));
    ASSERT_TRUE(succeeded(refresh()));

    const auto spec = readFile(specDir / "command.yaml");
    ASSERT_TRUE(spec);
    EXPECT_EQ(*spec, config.at("argv").at(2).get<std::string>());
    const auto cfg = readFile(cfgDir / "command.json");
    ASSERT_TRUE(cfg);
    EXPECT_EQ(json::parse(*cfg).at("devices").at(0).at("name"), "other.example.com/test=all");
}

TEST_F(CdiDrvRefreshTest, CommandTimeoutSkipsSourceAndReleasesRefreshLock)
{
    ASSERT_TRUE(writeBuiltin());
    fs::rename(configDir / "builtin.json", configDir / "healthy.json");
    const auto fifo = directory.path() / "blocked";
    const auto pidFile = directory.path() / "generator-pid";
    ASSERT_EQ(::mkfifo(fifo.c_str(), 0600), 0);
    for (const bool closeStdout : { false, true }) {
        SCOPED_TRACE(closeStdout);
        const std::string script = "echo $$ > \"$1\"; "
          + std::string(closeStdout ? "exec 1>&-; " : "") + "read value < \"$2\"";
        const json config{
            { "version", "1" },
            { "generator", "command" },
            { "timeout", 1 },
            { "argv",
              { "/bin/sh", "-c", script, "cdi-timeout-test", pidFile.string(), fifo.string() } }
        };
        ASSERT_TRUE(succeeded(writeFile(configDir / "command.json", config.dump())));
        ASSERT_TRUE(succeeded(run("refresh", { "--request-all" })));
        ASSERT_TRUE(succeeded(run("refresh", { "--request", "command" })));
        const auto startedAt = std::chrono::steady_clock::now();
        const auto result = refreshPending();
        ASSERT_TRUE(succeeded(result));
        EXPECT_THAT(result.error, HasSubstr("command timed out after 1000 ms"));
        const auto elapsed = std::chrono::steady_clock::now() - startedAt;
        EXPECT_GE(elapsed, std::chrono::seconds(1));
        EXPECT_LT(elapsed, std::chrono::seconds(5));
        EXPECT_TRUE(fs::exists(specDir / "healthy.json"));
        EXPECT_FALSE(entryExists(specDir / "command.yaml"));
        EXPECT_FALSE(entryExists(cfgDir / "command.json"));
        EXPECT_FALSE(entryExists(fullRequest));
        EXPECT_FALSE(entryExists(singleRequests / "command"));
        pid_t child = -1;
        ASSERT_TRUE(std::ifstream(pidFile) >> child);
        ASSERT_GT(child, 0);
        EXPECT_EQ(::kill(child, 0), -1);
        EXPECT_EQ(errno, ESRCH);

        // A subsequent refresh can acquire the lock and use the default timeout.
        ASSERT_TRUE(writeCommand());
        ASSERT_TRUE(succeeded(refresh({ "command" })));
        EXPECT_TRUE(fs::exists(specDir / "command.yaml"));
        EXPECT_TRUE(fs::exists(specDir / "healthy.json"));
    }
}

TEST_F(CdiDrvRefreshTest, CommandAllowsNonpositiveTimeout)
{
    for (const auto timeout : { 0, -1 }) {
        ASSERT_TRUE(writeCommand());
        const auto source = configDir / "command.json";
        const auto content = readFile(source);
        ASSERT_TRUE(content);
        auto config = json::parse(*content);
        config["timeout"] = timeout;
        ASSERT_TRUE(succeeded(writeFile(source, config.dump())));
        const auto result = refresh();
        ASSERT_TRUE(succeeded(result));
        EXPECT_THAT(result.error, Not(HasSubstr("timed out")));
        EXPECT_TRUE(fs::exists(specDir / "command.yaml"));
        EXPECT_TRUE(fs::exists(cfgDir / "command.json"));
    }
}

TEST_F(CdiDrvRefreshTest, NamedRefreshSwitchesCommandSpecFormat)
{
    ASSERT_TRUE(writeBuiltin());
    ASSERT_TRUE(writeCommand());
    ASSERT_TRUE(succeeded(refresh()));
    const auto builtin = specDir / "builtin.json";
    struct stat before{};
    ASSERT_EQ(::stat(builtin.c_str(), &before), 0);
    const auto yaml = readFile(specDir / "command.yaml");
    ASSERT_TRUE(yaml);
    const auto source = configDir / "command.json";
    const auto content = readFile(source);
    ASSERT_TRUE(content);
    auto config = json::parse(*content);
    const std::string output = R"json({"cdiVersion":"0.6.0","kind":"other.example.com/test",
        "devices":[{"name":"all","containerEdits":{"env":["COMMAND_ENV=value"]}}]})json";
    for (const auto &[specFilename, spec] :
         std::vector<std::pair<std::string, std::string>>{ { "command.json", output },
                                                           { "command.yaml", *yaml } }) {
        config["argv"][2] = spec;
        ASSERT_TRUE(succeeded(writeFile(source, config.dump())));
        ASSERT_TRUE(succeeded(refresh({ "command" })));
        const auto generated = readFile(specDir / specFilename);
        ASSERT_TRUE(generated);
        EXPECT_EQ(*generated, spec);
        EXPECT_FALSE(entryExists(
          specDir / (specFilename == "command.json" ? "command.yaml" : "command.json")));
        const auto cfg = readFile(cfgDir / "command.json");
        ASSERT_TRUE(cfg);
        EXPECT_EQ(json::parse(*cfg)["devices"][0]["spec"], "../spec/" + specFilename);
        struct stat after{};
        ASSERT_EQ(::stat(builtin.c_str(), &after), 0);
        EXPECT_EQ(before.st_ino, after.st_ino);
        const auto devices = linglong::cdi::getCDIDevices({ specDir.string() }, std::nullopt);
        ASSERT_TRUE(devices) << devices.error().message();
        EXPECT_EQ(devices->size(), 2);
        const auto generation = fs::read_symlink(current);
        ASSERT_TRUE(succeeded(refresh({ "command" })));
        EXPECT_EQ(fs::read_symlink(current), generation);
    }
}

TEST_F(CdiDrvRefreshTest, CommandDefaultsToFirstDeviceInSpec)
{
    const auto spec = R"yaml(cdiVersion: '0.6.0'
kind: example.com/gpu
devices:
  - name: gpuZ
    containerEdits: {env: [DEVICE=Z]}
  - name: gpuA
    containerEdits: {env: [DEVICE=A]}
)yaml";
    json config{ { "version", "1" },
                 { "generator", "command" },
                 { "argv", json::array({ "/usr/bin/printf", "%s", spec }) } };
    const auto source = configDir / "command.json";
    const auto cfg = cfgDir / "command.json";
    ASSERT_TRUE(succeeded(writeFile(source, config.dump())));
    ASSERT_TRUE(succeeded(refresh()));
    auto devices = readFile(cfg);
    ASSERT_TRUE(devices);
    EXPECT_EQ(json::parse(*devices).at("devices").at(0).at("name"), "example.com/gpu=gpuZ");

    config["defaultDevice"] = "gpuA";
    ASSERT_TRUE(succeeded(writeFile(source, config.dump())));
    ASSERT_TRUE(succeeded(refresh()));
    devices = readFile(cfg);
    ASSERT_TRUE(devices);
    EXPECT_EQ(json::parse(*devices).at("devices").at(0).at("name"), "example.com/gpu=gpuA");
}

TEST_F(CdiDrvRefreshTest, CommandUsesFirstYamlSpec)
{
    ASSERT_TRUE(writeCommand());
    const auto source = configDir / "command.json";
    auto content = readFile(source);
    ASSERT_TRUE(content);
    auto config = json::parse(*content);
    const auto output = config["argv"][2].get<std::string>() + R"yaml(---
cdiVersion: '0.6.0'
kind: second.example.com/test
devices:
  - name: another
    containerEdits: {env: [DEVICE=another]}
)yaml";
    config["argv"][2] = output;
    config.erase("defaultDevice");
    ASSERT_TRUE(succeeded(writeFile(source, config.dump())));

    ASSERT_TRUE(succeeded(refresh()));
    const auto cfg = readFile(cfgDir / "command.json");
    ASSERT_TRUE(cfg);
    EXPECT_EQ(json::parse(*cfg).at("devices").at(0).at("name"), "other.example.com/test=all");
    const auto spec = readFile(specDir / "command.yaml");
    ASSERT_TRUE(spec);
    EXPECT_EQ(*spec, output);
}

TEST(CdiSpecParserTest, ParsesJsonWithEscapedUnicode)
{
    const auto spec = linglong::cdi::parseCDISpec(R"json({
        "cdiVersion": "0.6.0",
        "kind": "example.com/gpu",
        "annotations": {"example.com/label": "\uD83D\uDE00"},
        "devices": [{"name": "gpu0", "containerEdits": {"env": ["GPU=enabled"]}}]
    })json");

    ASSERT_TRUE(spec) << spec.error().message();
    ASSERT_TRUE(spec->annotations);
    EXPECT_EQ(spec->annotations->at("example.com/label"), "\xF0\x9F\x98\x80");
}

TEST_F(CdiDrvRefreshTest, ReadsPublishedJsonWithEscapedUnicode)
{
    const std::string output = R"json({
        "cdiVersion": "0.6.0",
        "kind": "example.com/gpu",
        "devices": [{"name": "gpu0", "containerEdits": {"env": ["GPU=\uD83D\uDE00"]}}]
    })json";
    const json config{ { "version", "1" },
                       { "generator", "command" },
                       { "argv", json::array({ "/usr/bin/printf", "%s", output }) } };
    ASSERT_TRUE(succeeded(writeFile(configDir / "command.json", config.dump())));
    ASSERT_TRUE(succeeded(refresh()));
    const auto published = readFile(specDir / "command.json");
    ASSERT_TRUE(published);
    EXPECT_EQ(*published, output);

    linglong::cli::RunOptions options;
    options.cdiSpecDir.clear();
    const auto devices =
      linglong::runtime::resolveCdiDevices(linglong::runtime::loadCdiDriverGeneration(runtimeDir),
                                           std::nullopt,
                                           options);
    ASSERT_TRUE(devices) << devices.error().message();
    ASSERT_TRUE(*devices);
    ASSERT_EQ((**devices).size(), 1);
    const auto edits = linglong::cdi::getCDIDeviceEdits((**devices).front());
    ASSERT_TRUE(edits) << edits.error().message();
    ASSERT_TRUE(edits->env);
    EXPECT_EQ(*edits->env, (std::vector<std::string>{ "GPU=\xF0\x9F\x98\x80" }));
}

TEST(CdiSpecParserTest, ReadsJsonAndYamlIndependentlyOfFileSuffix)
{
    TempDir directory;
    const std::vector<std::string> contents{
        R"json({"cdiVersion":"0.6.0","kind":"example.com/gpu",
                 "devices":[{"name":"gpu0","containerEdits":{"env":["GPU=1"]}}]})json",
        R"yaml(cdiVersion: '0.6.0'
kind: example.com/gpu
devices:
  - name: gpu0
    containerEdits: {env: [GPU=1]}
)yaml"
    };
    for (const auto &content : contents) {
        for (const auto *filename : { "spec", "spec.json", "spec.yaml", "spec.data" }) {
            SCOPED_TRACE(filename);
            const auto path = directory.path() / filename;
            ASSERT_TRUE(succeeded(writeFile(path, content)));
            const auto devices = linglong::cdi::getCDIDevicesFromSpec(path);
            ASSERT_TRUE(devices) << devices.error().message();
            ASSERT_EQ(devices->size(), 1);
            const auto edits = linglong::cdi::getCDIDeviceEdits(devices->front());
            ASSERT_TRUE(edits) << edits.error().message();
            ASSERT_TRUE(edits->env);
            EXPECT_EQ(*edits->env, (std::vector<std::string>{ "GPU=1" }));
        }
    }
}

TEST(CdiSpecParserTest, DirectoryScanIgnoresBackupAndExtensionlessFiles)
{
    TempDir directory;
    const std::string content = R"json({"cdiVersion":"0.6.0","kind":"example.com/gpu",
        "devices":[{"name":"gpu0","containerEdits":{"env":["GPU=1"]}}]})json";
    for (const auto *filename : { "gpu.json~", "gpu.yaml.bak", "gpu.tmp", "gpu", ".gpu.swp" }) {
        ASSERT_TRUE(succeeded(writeFile(directory.path() / filename, content)));
    }
    for (const auto *extension : { ".json", ".yaml", ".yml" }) {
        const auto path = directory.path() / (std::string("gpu") + extension);
        ASSERT_TRUE(succeeded(writeFile(path, content)));
        linglong::cli::RunOptions options;
        options.cdiSpecDir = { directory.path().string() };
        options.cdiDevices = { "example.com/gpu=gpu0" };
        const auto devices = linglong::runtime::resolveCdiDevices({}, std::nullopt, options);
        ASSERT_TRUE(devices) << devices.error().message();
        ASSERT_TRUE(*devices);
        ASSERT_EQ((**devices).size(), 1);
        EXPECT_EQ((**devices).front().spec.path, path.string());
        EXPECT_TRUE(fs::remove(path));
    }
    const auto scanned = linglong::cdi::getCDIDevices({ directory.path().string() }, std::nullopt);
    ASSERT_TRUE(scanned) << scanned.error().message();
    EXPECT_TRUE(scanned->empty());
    EXPECT_TRUE(linglong::cdi::getCDIDevicesFromSpec(directory.path() / "gpu"));
    EXPECT_TRUE(linglong::cdi::getCDIDevicesFromSpec(directory.path() / "gpu.json~"));
}

TEST(CdiSpecParserTest, ValidatesFileAndContentSpecs)
{
    TempDir directory;
    const auto valid = json::parse(R"json({
        "cdiVersion": "0.6.0",
        "kind": "example.com/gpu",
        "devices": [{"name": "0:render", "containerEdits": {"env": ["GPU=enabled"]}}]
    })json");
    for (const auto *field : { "kind", "cdiVersion", "devices", "duplicate", "name" }) {
        SCOPED_TRACE(field);
        auto spec = valid;
        const std::string target(field);
        if (target == "devices") {
            spec["devices"] = json::array();
        } else if (target == "duplicate") {
            const auto device = spec["devices"][0];
            spec["devices"].push_back(device);
        } else if (target == "name") {
            spec["devices"][0]["name"] = "render-";
        } else {
            spec[target] = "";
        }
        EXPECT_FALSE(linglong::cdi::parseCDISpec(spec.dump()));
        for (const auto *filename : { "spec.json", "spec.yaml" }) {
            const auto path = directory.path() / filename;
            ASSERT_TRUE(succeeded(writeFile(path, spec.dump())));
            EXPECT_FALSE(linglong::cdi::getCDIDevicesFromSpec(path));
        }
    }
    EXPECT_TRUE(linglong::cdi::parseCDISpec(valid.dump()));
}

TEST_F(CdiDrvRefreshTest, BuiltinGeneratesMultipleDeviceProfiles)
{
    const auto source = configDir / "builtin.json";
    auto config = json::parse(R"json({
        "version": "1",
        "generator": "linglong",
        "kind": "example.com/test",
        "defaultDevice": "compute",
        "profile": {"env": ["GLOBAL=present", "MODE=base"]},
        "deviceProfile": [
            {"device": "render", "profile": {"env": ["MODE=render"]}},
            {"device": "compute", "profile": {"env": ["MODE=compute"]}}
        ]
    })json");
    ASSERT_TRUE(succeeded(writeFile(source, config.dump())));
    ASSERT_TRUE(succeeded(refresh()));
    const auto specContent = readFile(specDir / "builtin.json");
    ASSERT_TRUE(specContent);
    const auto spec = json::parse(*specContent);
    ASSERT_EQ(spec.at("devices").size(), 2);
    EXPECT_EQ(spec.at("devices").at(0).at("name"), "render");
    EXPECT_EQ(spec.at("devices").at(1).at("name"), "compute");
    EXPECT_EQ(spec.at("devices").at(1).at("containerEdits").at("env"),
              json::array({ "GLOBAL=present", "MODE=base", "MODE=compute" }));
    const auto cfg = readFile(cfgDir / "builtin.json");
    ASSERT_TRUE(cfg);
    EXPECT_EQ(json::parse(*cfg).at("devices").at(0).at("name"), "example.com/test=compute");

    config.erase("defaultDevice");
    ASSERT_TRUE(succeeded(writeFile(source, config.dump())));
    ASSERT_TRUE(succeeded(refresh()));
    const auto defaultCfg = readFile(cfgDir / "builtin.json");
    ASSERT_TRUE(defaultCfg);
    EXPECT_EQ(json::parse(*defaultCfg).at("devices").at(0).at("name"), "example.com/test=render");
}

TEST_F(CdiDrvRefreshTest, BuiltinRejectsDuplicateDeviceProfiles)
{
    ASSERT_TRUE(writeBuiltin());
    const auto source = configDir / "builtin.json";
    const auto content = readFile(source);
    ASSERT_TRUE(content);
    auto config = json::parse(*content);
    const auto profile = config["deviceProfile"][0];
    config["deviceProfile"].push_back(profile);
    config["deviceProfile"][1]["profile"]["deviceNodes"] = json::array({
      { { "path", (directory.path() / "missing-duplicate-device").string() } },
    });
    ASSERT_TRUE(succeeded(writeFile(source, config.dump())));
    const auto refreshed = refresh();
    ASSERT_TRUE(succeeded(refreshed));
    EXPECT_THAT(refreshed.error, HasSubstr("duplicate CDI driver device name: render"));
    EXPECT_THAT(refreshed.error, Not(HasSubstr("required path is missing")));
    EXPECT_FALSE(entryExists(specDir / "builtin.json"));
}

TEST_F(CdiDrvRefreshTest, BuiltinPreservesMappingOrderForRuntime)
{
    ASSERT_TRUE(writeBuiltin());
    const auto source = configDir / "builtin.json";
    const auto content = readFile(source);
    ASSERT_TRUE(content);
    auto config = json::parse(*content);
    auto &nodes = config["deviceProfile"][0]["profile"]["deviceNodes"];
    const auto globalNode = config["profile"]["deviceNodes"][0];
    nodes.push_back(globalNode);
    ASSERT_TRUE(succeeded(writeFile(source, config.dump())));
    ASSERT_TRUE(succeeded(refresh()));
    const auto spec = readFile(specDir / "builtin.json");
    ASSERT_TRUE(spec);
    EXPECT_EQ(json::parse(*spec).at("devices").at(0).at("containerEdits").at("deviceNodes").size(),
              3);

    nodes.back()["hostPath"] = "/dev/zero";
    ASSERT_TRUE(succeeded(writeFile(source, config.dump())));
    ASSERT_TRUE(succeeded(refresh()));
    const auto updated = readFile(specDir / "builtin.json");
    ASSERT_TRUE(updated);
    const auto updatedNodes =
      json::parse(*updated).at("devices").at(0).at("containerEdits").at("deviceNodes");
    ASSERT_EQ(updatedNodes.size(), 3);
    EXPECT_EQ(updatedNodes.at(0).at("path"), "/dev/test");
    EXPECT_EQ(updatedNodes.at(0).at("hostPath"), "/dev/null");
    EXPECT_EQ(updatedNodes.at(2).at("path"), "/dev/test");
    EXPECT_EQ(updatedNodes.at(2).at("hostPath"), "/dev/zero");
}

TEST_F(CdiDrvRefreshTest, BuiltinUsesLibraryDirectoryFromEnvironment)
{
    ASSERT_TRUE(writeBuiltin());
    const auto libraryDir = directory.path() / "driver-libraries";
    ASSERT_TRUE(fs::create_directory(libraryDir));
    const auto library = libraryDir / "test-driver.so";
    ASSERT_TRUE(succeeded(writeFile(library, "driver data")));
    const auto source = configDir / "builtin.json";
    const auto content = readFile(source);
    ASSERT_TRUE(content);
    auto config = json::parse(*content);
    config["profile"]["mountFiles"] = json::array({ "${LIB_DIR}/test-driver.so" });
    ASSERT_TRUE(succeeded(writeFile(source, config.dump())));

    const std::map<std::string, std::string> environment{ { "LINGLONG_CDI_DRV_LIB_DIR",
                                                            libraryDir.string() } };
    ASSERT_TRUE(succeeded(runCli({ "refresh",
                                   "--runtime-dir",
                                   runtimeDir.c_str(),
                                   "--config-dir",
                                   configDir.c_str(),
                                   "--unit-dir",
                                   unitDir.c_str(),
                                   "--systemctl",
                                   systemctl.c_str() },
                                 environment)));
    const auto spec = readFile(specDir / "builtin.json");
    ASSERT_TRUE(spec);
    const auto mount =
      json::parse(*spec).at("devices").at(0).at("containerEdits").at("mounts").at(0);
    EXPECT_EQ(mount.at("hostPath"), library.string());
    EXPECT_EQ(mount.at("containerPath"), library.string());
}

TEST_F(CdiDrvRefreshTest, BuiltinEscapesLibraryDirectoryVariables)
{
    ASSERT_TRUE(writeBuiltin());
    const auto libraryDir = directory.path() / "${LIB_DIR}";
    ASSERT_TRUE(fs::create_directory(libraryDir));
    const std::vector<std::pair<std::string, std::string>> paths{
        { "${LIB_DIR}/$${LIB_DIR}.so", "${LIB_DIR}.so" },
        { "${LIB_DIR}/driver$$.so", "driver$.so" },
        { "${LIB_DIR}/driver$", "driver$" },
        { "${LIB_DIR}/$OTHER.so", "$OTHER.so" },
        { "${LIB_DIR}/$$$${LIB_DIR}.so", "$${LIB_DIR}.so" },
    };
    const auto source = configDir / "builtin.json";
    const auto content = readFile(source);
    ASSERT_TRUE(content);
    auto config = json::parse(*content);
    config["profile"]["mountFiles"] = json::array();
    for (const auto &[input, filename] : paths) {
        ASSERT_TRUE(succeeded(writeFile(libraryDir / filename, "driver data")));
        config["profile"]["mountFiles"].push_back(input);
    }
    ASSERT_TRUE(succeeded(writeFile(source, config.dump())));
    ASSERT_TRUE(succeeded(runCli({ "refresh",
                                   "--runtime-dir",
                                   runtimeDir.c_str(),
                                   "--config-dir",
                                   configDir.c_str(),
                                   "--unit-dir",
                                   unitDir.c_str(),
                                   "--systemctl",
                                   systemctl.c_str() },
                                 { { "LINGLONG_CDI_DRV_LIB_DIR", libraryDir.string() } })));
    const auto spec = readFile(specDir / "builtin.json");
    ASSERT_TRUE(spec);
    const auto mounts = json::parse(*spec).at("devices").at(0).at("containerEdits").at("mounts");
    ASSERT_EQ(mounts.size(), paths.size());
    for (std::size_t index = 0; index < paths.size(); ++index) {
        const auto expected = (libraryDir / paths[index].second).string();
        EXPECT_EQ(mounts.at(index).at("hostPath"), expected);
        EXPECT_EQ(mounts.at(index).at("containerPath"), expected);
    }
}

TEST_F(CdiDrvRefreshTest, BuiltinAcceptsExistingDeviceMountSources)
{
    ASSERT_TRUE(writeBuiltin());
    const auto fifo = directory.path() / "device-fifo";
    ASSERT_EQ(::mkfifo(fifo.c_str(), 0600), 0);
    const auto file = directory.path() / "device-file";
    ASSERT_TRUE(succeeded(writeFile(file, "device data")));
    const auto folder = directory.path() / "device-directory";
    ASSERT_TRUE(fs::create_directory(folder));
    const auto source = configDir / "builtin.json";
    const auto content = readFile(source);
    ASSERT_TRUE(content);
    auto config = json::parse(*content);
    config["profile"]["deviceNodes"] = json::array({
      { { "path", "/dev/test-fifo" }, { "hostPath", fifo.string() } },
      { { "path", "/dev/test-file" }, { "hostPath", file.string() } },
      { { "path", "/dev/test-directory" }, { "hostPath", folder.string() } },
    });
    ASSERT_TRUE(succeeded(writeFile(source, config.dump())));
    ASSERT_TRUE(succeeded(refresh()));
    const auto spec = readFile(specDir / "builtin.json");
    ASSERT_TRUE(spec);
    const auto nodes =
      json::parse(*spec).at("devices").at(0).at("containerEdits").at("deviceNodes");
    ASSERT_EQ(nodes.size(), 4);
    EXPECT_EQ(nodes.at(0).at("path"), "/dev/test-fifo");
    EXPECT_EQ(nodes.at(0).at("hostPath"), fifo.string());
    EXPECT_EQ(nodes.at(1).at("path"), "/dev/test-file");
    EXPECT_EQ(nodes.at(1).at("hostPath"), file.string());
    EXPECT_EQ(nodes.at(2).at("path"), "/dev/test-directory");
    EXPECT_EQ(nodes.at(2).at("hostPath"), folder.string());
}

TEST_F(CdiDrvRefreshTest, BuiltinAcceptsDirectoryAndFifoMountSources)
{
    ASSERT_TRUE(writeBuiltin());
    const auto folder = directory.path() / "mount-directory";
    ASSERT_TRUE(fs::create_directory(folder));
    const auto fifo = directory.path() / "mount-fifo";
    ASSERT_EQ(::mkfifo(fifo.c_str(), 0600), 0);
    const auto source = configDir / "builtin.json";
    const auto content = readFile(source);
    ASSERT_TRUE(content);
    auto config = json::parse(*content);
    config["profile"]["mountFiles"] = json::array({
      folder.string(),
      { { "hostPath", fifo.string() }, { "path", "/run/test-fifo" } },
      { { "hostPath", (directory.path() / "missing-mount").string() }, { "required", false } },
    });
    ASSERT_TRUE(succeeded(writeFile(source, config.dump())));
    ASSERT_TRUE(succeeded(refresh()));
    const auto spec = readFile(specDir / "builtin.json");
    ASSERT_TRUE(spec);
    const auto mounts = json::parse(*spec).at("devices").at(0).at("containerEdits").at("mounts");
    ASSERT_EQ(mounts.size(), 2);
    EXPECT_EQ(mounts.at(0).at("hostPath"), folder.string());
    EXPECT_EQ(mounts.at(0).at("containerPath"), folder.string());
    EXPECT_EQ(mounts.at(1).at("hostPath"), fifo.string());
    EXPECT_EQ(mounts.at(1).at("containerPath"), "/run/test-fifo");
    for (const auto &mount : mounts) {
        EXPECT_EQ(mount.at("options"),
                  json::array({ "ro", "nosuid", "nodev", "rbind", "rprivate" }));
        EXPECT_EQ(mount.at("type"), "bind");
    }
}

TEST_F(CdiDrvRefreshTest, BuiltinPreservesHostPathsContainingSymlinks)
{
    const auto real = directory.path() / "real";
    ASSERT_TRUE(fs::create_directories(real / "nested"));
    fs::create_directory_symlink(real / "nested", directory.path() / "alias");
    fs::create_symlink("/dev/null", real / "device");
    ASSERT_TRUE(succeeded(writeFile(real / "driver.so", "driver data")));
    const auto device = (directory.path() / "alias/../device").string();
    const auto library = (directory.path() / "alias/../driver.so").string();
    const json config{
        { "version", "1" },
        { "generator", "linglong" },
        { "kind", "example.com/test" },
        { "profile",
          { { "deviceNodes", json::array({ { { "path", "/dev/test" }, { "hostPath", device } } }) },
            { "mountFiles",
              json::array({ { { "path", "/usr/lib/driver.so" }, { "hostPath", library } } }) } } }
    };
    ASSERT_TRUE(succeeded(writeFile(configDir / "builtin.json", config.dump())));
    ASSERT_TRUE(succeeded(refresh()));
    const auto spec = readFile(specDir / "builtin.json");
    ASSERT_TRUE(spec);
    const auto edits = json::parse(*spec).at("devices").at(0).at("containerEdits");
    EXPECT_EQ(edits.at("deviceNodes").at(0).at("hostPath"), device);
    EXPECT_EQ(edits.at("mounts").at(0).at("hostPath"), library);
}

TEST_F(CdiDrvRefreshTest, BuiltinInheritsAndOverridesGlobalDrm)
{
    ASSERT_TRUE(writeBuiltin());
    const auto source = configDir / "builtin.json";
    const auto content = readFile(source);
    ASSERT_TRUE(content);
    auto config = json::parse(*content);
    config["profile"]["drm"] = { { "kernelDriver", "" } };
    ASSERT_TRUE(succeeded(writeFile(source, config.dump())));
    const auto refreshed = refresh();
    ASSERT_TRUE(succeeded(refreshed));
    EXPECT_THAT(refreshed.error, HasSubstr("empty DRM kernelDriver"));
    EXPECT_FALSE(entryExists(specDir / "builtin.json"));

    config["deviceProfile"][0]["profile"]["drm"] = { { "nodes", json::array() } };
    ASSERT_TRUE(succeeded(writeFile(source, config.dump())));
    const auto overridden = refresh();
    ASSERT_TRUE(succeeded(overridden));
    EXPECT_THAT(overridden.error, Not(HasSubstr("empty DRM kernelDriver")));
    const auto overriddenSpec = readFile(specDir / "builtin.json");
    ASSERT_TRUE(overriddenSpec);
    const auto overriddenEdits =
      json::parse(*overriddenSpec).at("devices").at(0).at("containerEdits");
    EXPECT_EQ(overriddenEdits.at("deviceNodes").size(), 2);
    EXPECT_EQ(overriddenEdits.at("mounts").size(), 1);
    EXPECT_EQ(overriddenEdits.at("env"),
              json::array({ "GLOBAL=present", "MODE=base", "MODE=render" }));

    config["deviceProfile"][0]["profile"]["drm"] = { { "kernelDriver",
                                                       "ll-cdi-drv-nonexistent-test-driver" } };
    ASSERT_TRUE(succeeded(writeFile(source, config.dump())));
    ASSERT_TRUE(succeeded(refresh()));
    const auto spec = readFile(specDir / "builtin.json");
    ASSERT_TRUE(spec);
    const auto edits = json::parse(*spec).at("devices").at(0).at("containerEdits");
    EXPECT_EQ(edits.at("deviceNodes").size(), 2);
    EXPECT_EQ(edits.at("env"), json::array({ "GLOBAL=present", "MODE=base", "MODE=render" }));
}

TEST_F(CdiDrvRefreshTest, BuiltinAllowsDrmWithoutKernelDriver)
{
    ASSERT_TRUE(writeBuiltin());
    const auto source = configDir / "builtin.json";
    const auto content = readFile(source);
    ASSERT_TRUE(content);
    auto config = json::parse(*content);
    for (const auto &selector : { json::object(),
                                  json{ { "pciIds", json::array() } },
                                  json{ { "pciIds", json::array({ "0000:0000" }) } } }) {
        SCOPED_TRACE(selector.dump());
        config["profile"]["drm"] = selector;
        ASSERT_TRUE(succeeded(writeFile(source, config.dump())));
        const auto refreshed = refresh();
        ASSERT_TRUE(succeeded(refreshed));
        EXPECT_THAT(refreshed.error, Not(HasSubstr("kernelDriver")));
        const auto spec = readFile(specDir / "builtin.json");
        ASSERT_TRUE(spec);
        const auto edits = json::parse(*spec).at("devices").at(0).at("containerEdits");
        EXPECT_GE(edits.at("deviceNodes").size(), 2);
        EXPECT_EQ(edits.at("env"), json::array({ "GLOBAL=present", "MODE=base", "MODE=render" }));
    }
}

TEST_F(CdiDrvRefreshTest, BuiltinValidatesPciIds)
{
    ASSERT_TRUE(writeBuiltin());
    const auto source = configDir / "builtin.json";
    const auto content = readFile(source);
    ASSERT_TRUE(content);
    auto config = json::parse(*content);
    config["profile"]["drm"] = { { "kernelDriver", "ll-cdi-drv-test-driver" } };
    for (const auto &ids :
         { json::array(), json::array({ "0000:0000", "FFFF:FFFF", "aBcD:eF01", "10de:*" }) }) {
        SCOPED_TRACE(ids.dump());
        config["profile"]["drm"]["pciIds"] = ids;
        ASSERT_TRUE(succeeded(writeFile(source, config.dump())));
        const auto refreshed = refresh();
        ASSERT_TRUE(succeeded(refreshed));
        EXPECT_THAT(refreshed.error, Not(HasSubstr("invalid PCI IDs")));
        EXPECT_TRUE(entryExists(specDir / "builtin.json"));
    }

    for (const auto &ids : { json::array({ "" }),
                             json::array({ "123:1234" }),
                             json::array({ "12345:1234" }),
                             json::array({ "1234/1234" }),
                             json::array({ "1234:1" }),
                             json::array({ "1234:123" }),
                             json::array({ "1234:12345" }),
                             json::array({ "*:1234" }),
                             json::array({ "123g:1234" }),
                             json::array({ "1234:12G4" }),
                             json::array({ "1234:****" }),
                             json::array({ "1234:1234\n" }),
                             json::array({ "10de:*", "1234:?" }) }) {
        SCOPED_TRACE(ids.dump());
        config["profile"]["drm"]["pciIds"] = ids;
        ASSERT_TRUE(succeeded(writeFile(source, config.dump())));
        const auto rejected = refresh();
        ASSERT_TRUE(succeeded(rejected));
        EXPECT_THAT(rejected.error, HasSubstr("invalid PCI IDs in DRM selector"));
        EXPECT_FALSE(entryExists(specDir / "builtin.json"));
    }
}

TEST_F(CdiDrvRefreshTest, BuiltinRejectsMalformedDrmNodes)
{
    ASSERT_TRUE(writeBuiltin());
    const auto source = configDir / "builtin.json";
    const auto content = readFile(source);
    ASSERT_TRUE(content);
    const auto config = json::parse(*content);
    for (const bool deviceLevel : { false, true }) {
        auto invalid = config;
        auto &profile = deviceLevel ? invalid["deviceProfile"][0]["profile"] : invalid["profile"];
        profile["drm"] = { { "nodes", "render" } };
        ASSERT_TRUE(succeeded(writeFile(source, invalid.dump())));
        const auto refreshed = refresh();
        ASSERT_TRUE(succeeded(refreshed));
        EXPECT_THAT(refreshed.error, HasSubstr("Skipping CDI source"));
        EXPECT_FALSE(entryExists(specDir / "builtin.json"));
    }
}

TEST_F(CdiDrvRefreshTest, BuiltinRejectsMissingProfileWithAbsentOrEmptyDeviceProfiles)
{
    auto config = json::parse(R"json({
        "version": "1",
        "generator": "linglong",
        "kind": "example.com/test"
    })json");
    for (const bool emptyArray : { false, true }) {
        SCOPED_TRACE(emptyArray);
        if (emptyArray) {
            config["deviceProfile"] = json::array();
        }
        ASSERT_TRUE(succeeded(writeFile(configDir / "builtin.json", config.dump())));
        const auto refreshed = refresh();
        ASSERT_TRUE(succeeded(refreshed));
        EXPECT_THAT(refreshed.error, HasSubstr("linglong generator requires a kind and a profile"));
        EXPECT_FALSE(entryExists(specDir / "builtin.json"));
        EXPECT_FALSE(entryExists(cfgDir / "builtin.json"));
    }
}

TEST_F(CdiDrvRefreshTest, BuiltinUsesDefaultNameWithoutDeviceProfiles)
{
    auto config = json::parse(R"json({
        "version": "1",
        "generator": "linglong",
        "kind": "example.com/test",
        "profile": {"env": ["MODE=base"]}
    })json");
    for (const bool emptyArray : { false, true }) {
        if (emptyArray) {
            config["deviceProfile"] = json::array();
        }
        ASSERT_TRUE(succeeded(writeFile(configDir / "builtin.json", config.dump())));
        ASSERT_TRUE(succeeded(refresh()));
        const auto cfg = readFile(cfgDir / "builtin.json");
        ASSERT_TRUE(cfg);
        EXPECT_EQ(json::parse(*cfg).at("devices").at(0).at("name"),
                  "example.com/test=builtin-linglong");
    }
}

TEST_F(CdiDrvRefreshTest, BuiltinUsesConfiguredDefaultNameWithoutDeviceProfiles)
{
    auto config = json::parse(R"json({
        "version": "1",
        "generator": "linglong",
        "kind": "example.com/test",
        "defaultDevice": "render.0_1-2:3",
        "profile": {"env": ["MODE=base"]}
    })json");
    for (const bool emptyArray : { false, true }) {
        SCOPED_TRACE(emptyArray);
        if (emptyArray) {
            config["deviceProfile"] = json::array();
        }
        ASSERT_TRUE(succeeded(writeFile(configDir / "_gpu.json", config.dump())));
        ASSERT_TRUE(succeeded(refresh()));
        const auto specContent = readFile(specDir / "_gpu.json");
        ASSERT_TRUE(specContent);
        const auto spec = json::parse(*specContent);
        ASSERT_EQ(spec.at("devices").size(), 1);
        EXPECT_EQ(spec.at("devices").at(0).at("name"), "render.0_1-2:3");
        EXPECT_EQ(spec.at("devices").at(0).at("containerEdits").at("env"),
                  json::array({ "MODE=base" }));
        const auto cfg = readFile(cfgDir / "_gpu.json");
        ASSERT_TRUE(cfg);
        EXPECT_EQ(json::parse(*cfg).at("devices").at(0).at("name"),
                  "example.com/test=render.0_1-2:3");
        EXPECT_EQ(json::parse(*cfg).at("devices").at(0).at("spec"), "../spec/_gpu.json");
    }
}

TEST_F(CdiDrvRefreshTest, BuiltinRejectsInvalidDefaultNameWithoutDeviceProfiles)
{
    auto config = json::parse(R"json({
        "version": "1",
        "generator": "linglong",
        "kind": "example.com/test",
        "profile": {"env": ["MODE=base"]}
    })json");
    for (const bool emptyArray : { false, true }) {
        SCOPED_TRACE(emptyArray);
        if (emptyArray) {
            config["deviceProfile"] = json::array();
        }
        for (const auto *name : { "", "_render", "-render", "render/0", "render$0", "render-" }) {
            SCOPED_TRACE(name);
            config["defaultDevice"] = name;
            ASSERT_TRUE(succeeded(writeFile(configDir / "builtin.json", config.dump())));
            const auto refreshed = refresh();
            ASSERT_TRUE(succeeded(refreshed));
            EXPECT_THAT(refreshed.error, HasSubstr("invalid CDI driver device name:"));
            EXPECT_FALSE(entryExists(specDir / "builtin.json"));
            EXPECT_FALSE(entryExists(cfgDir / "builtin.json"));
        }
    }
}

TEST_F(CdiDrvRefreshTest, BuiltinRejectsUnknownDefaultDeviceWithDeviceProfiles)
{
    ASSERT_TRUE(writeBuiltin());
    const auto source = configDir / "builtin.json";
    const auto content = readFile(source);
    ASSERT_TRUE(content);
    auto config = json::parse(*content);
    config["defaultDevice"] = "compute";
    ASSERT_TRUE(succeeded(writeFile(source, config.dump())));
    const auto refreshed = refresh();
    ASSERT_TRUE(succeeded(refreshed));
    EXPECT_THAT(refreshed.error, HasSubstr("defaultDevice does not name a configured device"));
    EXPECT_FALSE(entryExists(specDir / "builtin.json"));
    EXPECT_FALSE(entryExists(cfgDir / "builtin.json"));
}

TEST_F(CdiDrvRefreshTest, BuiltinIgnoresCommandArguments)
{
    ASSERT_TRUE(writeBuiltin());
    const auto source = configDir / "builtin.json";
    const auto content = readFile(source);
    ASSERT_TRUE(content);
    auto config = json::parse(*content);
    config["timeout"] = 0;
    for (const auto &argv : { json::array(), json::array({ "/missing/cdi-command" }) }) {
        config["argv"] = argv;
        ASSERT_TRUE(succeeded(writeFile(source, config.dump())));
        ASSERT_TRUE(succeeded(refresh()));
        const auto cfg = readFile(cfgDir / "builtin.json");
        ASSERT_TRUE(cfg);
        EXPECT_EQ(json::parse(*cfg).at("devices").at(0).at("name"), "example.com/test=render");
    }
}

TEST_F(CdiDrvRefreshTest, ValidatesFinalBuiltinDeviceNames)
{
    ASSERT_TRUE(writeBuiltin());
    const auto source = configDir / "builtin.json";
    auto content = readFile(source);
    ASSERT_TRUE(content);
    auto config = json::parse(*content);
    for (const auto *name : { "",
                              "_render",
                              "-render",
                              ".render",
                              ":render",
                              "render-",
                              "render_",
                              "render.",
                              "render:",
                              "render/card" }) {
        SCOPED_TRACE(name);
        config["defaultDevice"] = name;
        config["deviceProfile"][0]["device"] = name;
        ASSERT_TRUE(succeeded(writeFile(source, config.dump())));
        const auto refreshed = refresh();
        ASSERT_TRUE(succeeded(refreshed));
        EXPECT_THAT(refreshed.error, HasSubstr("invalid CDI driver device name:"));
        EXPECT_FALSE(entryExists(specDir / "builtin.json"));
    }

    for (const auto *name : { "render", "render_0", "render-0", "render.0", "0.A_b-c:1" }) {
        SCOPED_TRACE(name);
        config["defaultDevice"] = name;
        config["deviceProfile"][0]["device"] = name;
        ASSERT_TRUE(succeeded(writeFile(source, config.dump())));
        ASSERT_TRUE(succeeded(refresh()));
        const auto cfg = readFile(cfgDir / "builtin.json");
        ASSERT_TRUE(cfg);
        EXPECT_EQ(json::parse(*cfg).at("devices").at(0).at("name"),
                  std::string("example.com/test=") + name);
    }
}

TEST_F(CdiDrvRefreshTest, ValidatesCommandDeviceNames)
{
    for (const auto *name : { "", "-gpu", "gpu-", "gpu_", "gpu.", "gpu:" }) {
        SCOPED_TRACE(name);
        ASSERT_TRUE(writeCommand(name));
        ASSERT_TRUE(succeeded(refresh()));
        EXPECT_FALSE(entryExists(specDir / "command.yaml"));
    }

    for (const auto *name : { "0", "gpu-0.1_2", "mig0:0" }) {
        SCOPED_TRACE(name);
        ASSERT_TRUE(writeCommand(name, "0.4.0"));
        ASSERT_TRUE(succeeded(refresh()));
        const auto cfg = readFile(cfgDir / "command.json");
        ASSERT_TRUE(cfg);
        EXPECT_THAT(*cfg, HasSubstr(std::string("other.example.com/test=") + name));
    }
}

TEST_F(CdiDrvRefreshTest, ValidatesCdiKindSegments)
{
    json config{ { "version", "1" },
                 { "generator", "linglong" },
                 { "profile", { { "env", { "GPU=enabled" } } } } };
    for (const auto *kind : { "example.com",
                              "/gpu",
                              "example.com/",
                              "example.com/gpu/render",
                              "example.com/_gpu",
                              "example.com/gpu:0" }) {
        SCOPED_TRACE(kind);
        config["kind"] = kind;
        ASSERT_TRUE(succeeded(writeFile(configDir / "builtin.json", config.dump())));
        const auto refreshed = refresh();
        ASSERT_TRUE(succeeded(refreshed));
        EXPECT_THAT(refreshed.error, HasSubstr("invalid CDI driver kind:"));
        EXPECT_FALSE(entryExists(specDir / "builtin.json"));
    }

    config["kind"] = "example.com/gpu-render_1.2";
    ASSERT_TRUE(succeeded(writeFile(configDir / "builtin.json", config.dump())));
    ASSERT_TRUE(succeeded(refresh()));
    EXPECT_TRUE(fs::is_regular_file(specDir / "builtin.json"));
}

TEST_F(CdiDrvRefreshTest, NamedRefreshUpdatesOnlyItsOwnArtifacts)
{
    for (const bool pending : { false, true }) {
        SCOPED_TRACE(pending);
        ASSERT_TRUE(writeBuiltin());
        ASSERT_TRUE(writeCommand());
        ASSERT_TRUE(succeeded(refresh()));
        const auto previous = fs::read_symlink(current);

        // The missing required node invalidates the named profile only.
        ASSERT_TRUE(writeBuiltin(true));
        ASSERT_TRUE(fs::remove(configDir / "command.json"));
        if (pending) {
            ASSERT_TRUE(succeeded(run("refresh", { "--request", "builtin" })));
            ASSERT_TRUE(succeeded(refreshPending()));
        } else {
            ASSERT_TRUE(succeeded(refresh({ "builtin" })));
        }
        EXPECT_NE(fs::read_symlink(current), previous);

        EXPECT_FALSE(entryExists(cfgDir / "builtin.json"));
        EXPECT_FALSE(entryExists(specDir / "builtin.json"));
        const auto oldSpec = readFile(runtimeDir / previous / "spec/builtin.json");
        ASSERT_TRUE(oldSpec);
        EXPECT_THAT(*oldSpec, HasSubstr("\"name\":\"render\""));
        const auto unaffected = readFile(cfgDir / "command.json");
        ASSERT_TRUE(unaffected);
        EXPECT_THAT(*unaffected, HasSubstr("other.example.com/test=all"));
    }
}

TEST_F(CdiDrvRefreshTest, DoesNotRequeueAfterPublicationFailure)
{
    ASSERT_TRUE(writeBuiltin());
    ASSERT_TRUE(writeCommand());
    ASSERT_TRUE(succeeded(refresh()));
    ASSERT_TRUE(fs::remove(configDir / "command.json"));

    ASSERT_TRUE(succeeded(run("refresh", { "--request-all" })));
    ASSERT_TRUE(succeeded(run("refresh", { "--request", "builtin" })));
    ASSERT_TRUE(fs::is_regular_file(specDir / "command.yaml"));

    ASSERT_TRUE(fs::remove(cfgDir));
    fs::create_symlink("wrong-target", cfgDir);
    EXPECT_GT(refreshPending().status, 0);
    EXPECT_FALSE(entryExists(fullRequest));
    EXPECT_FALSE(entryExists(singleRequests / "builtin"));
    EXPECT_TRUE(fs::is_regular_file(specDir / "command.yaml"));

    ASSERT_TRUE(succeeded(refreshPending()));
    EXPECT_TRUE(fs::is_regular_file(specDir / "command.yaml"));

    ASSERT_TRUE(fs::remove(cfgDir));
    fs::create_symlink("current/cfg", cfgDir);
    ASSERT_TRUE(succeeded(run("refresh", { "--request-all" })));
    ASSERT_TRUE(succeeded(refreshPending()));
    EXPECT_FALSE(entryExists(specDir / "command.yaml"));
    EXPECT_FALSE(entryExists(cfgDir / "command.json"));
    EXPECT_FALSE(entryExists(fullRequest));
    EXPECT_FALSE(entryExists(singleRequests / "builtin"));
    EXPECT_FALSE(entryExists(runtimeDir / "processing"));
}

TEST_F(CdiDrvRefreshTest, ReturnsImmediatelyWhenClaimingRequestsFails)
{
    ASSERT_TRUE(succeeded(run("refresh", { "--request-all" })));
    ASSERT_TRUE(fs::remove(runtimeDir / "requests.lock"));
    ASSERT_TRUE(fs::create_directory(runtimeDir / "requests.lock"));

    const auto startedAt = std::chrono::steady_clock::now();
    const auto result = run("refresh-pending", { "--idle-timeout", "5" });
    EXPECT_GT(result.status, 0);
    EXPECT_THAT(result.error, HasSubstr("open CDI requests lock"));
    EXPECT_LT(std::chrono::steady_clock::now() - startedAt, std::chrono::seconds(2));
    EXPECT_TRUE(entryExists(fullRequest));
    EXPECT_FALSE(entryExists(current));
}

TEST_F(CdiDrvRefreshTest, UnpublishesWhenLastSourceIsRemoved)
{
    ASSERT_TRUE(writeBuiltin());
    ASSERT_TRUE(succeeded(refresh()));
    const auto previous = generations();
    ASSERT_TRUE(fs::is_symlink(current));

    ASSERT_TRUE(fs::remove(configDir / "builtin.json"));
    ASSERT_TRUE(succeeded(refresh()));
    EXPECT_EQ(generations(), previous);
    EXPECT_FALSE(entryExists(current));
    EXPECT_FALSE(fs::exists(specDir));
    EXPECT_FALSE(fs::exists(cfgDir));

    ASSERT_TRUE(writeBuiltin());
    ASSERT_TRUE(succeeded(refresh()));
    const auto cfg = readFile(cfgDir / "builtin.json");
    ASSERT_TRUE(cfg);
    EXPECT_THAT(*cfg, HasSubstr("example.com/test=render"));
}

} // namespace
