/*
 * SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "linglong/oci-cfg-generators/container_cfg_builder.h"
#include "ocppi/runtime/config/types/NamespaceType.hpp"

#include <common/tempdir.h>

#include <filesystem>
#include <fstream>
#include <vector>

using namespace linglong::generator;
using ocppi::runtime::config::types::Config;
using ocppi::runtime::config::types::Mount;

namespace {

class ContainerCfgBuilderTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        ASSERT_TRUE(baseDir.isValid());
        ASSERT_TRUE(bundleDir.isValid());
        ASSERT_TRUE(appDir.isValid());
        ASSERT_TRUE(runtimeDir.isValid());
        // mkdtemp already created the directories
        ASSERT_TRUE(std::filesystem::is_directory(baseDir.path()));
        ASSERT_TRUE(std::filesystem::is_directory(bundleDir.path()));
        ASSERT_TRUE(std::filesystem::is_directory(appDir.path()));
        ASSERT_TRUE(std::filesystem::is_directory(runtimeDir.path()));
    }

    TempDir baseDir{ "ll-cfg-base-" };
    TempDir bundleDir{ "ll-cfg-bundle-" };
    TempDir appDir{ "ll-cfg-app-" };
    TempDir runtimeDir{ "ll-cfg-runtime-" };
};

TEST_F(ContainerCfgBuilderTest, BuildWithRequiredFields)
{
    ContainerCfgBuilder builder;
    builder.setAppId("org.deepin.demo")
      .setBasePath(baseDir.path())
      .setBundlePath(bundleDir.path())
      .setContainerId("fake-container-id")
      .disablePatch();

    auto result = builder.build();
    ASSERT_TRUE(result.has_value()) << result.error().message();

    const auto &config = builder.getConfig();
    EXPECT_EQ(config.ociVersion, "1.0.1");
    EXPECT_EQ(config.hostname, "linglong");
    ASSERT_TRUE(config.root.has_value());
    EXPECT_EQ(config.root->path, baseDir.path());
    EXPECT_EQ(builder.getContainerId(), "fake-container-id");

    ASSERT_TRUE(config.linux_.has_value());
    ASSERT_TRUE(config.linux_->namespaces.has_value());
    bool hasUserNs = false;
    for (const auto &ns : *config.linux_->namespaces) {
        if (ns.type == ocppi::runtime::config::types::NamespaceType::User) {
            hasUserNs = true;
        }
    }
    EXPECT_TRUE(hasUserNs);
}

TEST_F(ContainerCfgBuilderTest, CheckValidRejectsEmptyAppId)
{
    ContainerCfgBuilder builder;
    builder.setBasePath(baseDir.path()).setBundlePath(bundleDir.path());
    auto result = builder.build();
    ASSERT_FALSE(result.has_value());
    EXPECT_THAT(result.error().message(), ::testing::HasSubstr("app id is empty"));
}

TEST_F(ContainerCfgBuilderTest, CheckValidRejectsEmptyBasePath)
{
    ContainerCfgBuilder builder;
    builder.setAppId("org.deepin.demo").setBundlePath(bundleDir.path());
    auto result = builder.build();
    ASSERT_FALSE(result.has_value());
    EXPECT_THAT(result.error().message(), ::testing::HasSubstr("base path is not set"));
}

TEST_F(ContainerCfgBuilderTest, CheckValidRejectsEmptyBundlePath)
{
    ContainerCfgBuilder builder;
    builder.setAppId("org.deepin.demo").setBasePath(baseDir.path());
    auto result = builder.build();
    ASSERT_FALSE(result.has_value());
    EXPECT_THAT(result.error().message(), ::testing::HasSubstr("bundle path is empty"));
}

TEST_F(ContainerCfgBuilderTest, RejectsOverlayWithSelfAdjustingMount)
{
    ContainerCfgBuilder builder;
    builder.setAppId("org.deepin.demo")
      .setBasePath(baseDir.path())
      .setBundlePath(bundleDir.path())
      .enableOverlayMode(baseDir.path() / "merged", true)
      .enableSelfAdjustingMount();
    auto result = builder.build();
    ASSERT_FALSE(result.has_value());
    EXPECT_THAT(result.error().message(), ::testing::HasSubstr("overlay and self-adjusting"));
}

TEST_F(ContainerCfgBuilderTest, OverlayModeSetsRoot)
{
    ContainerCfgBuilder builder;
    builder.setAppId("org.deepin.demo")
      .setBasePath(baseDir.path())
      .setBundlePath(bundleDir.path())
      .enableOverlayMode(baseDir.path() / "merged", true);

    auto result = builder.build();
    ASSERT_TRUE(result.has_value()) << result.error().message();
    ASSERT_TRUE(builder.getConfig().root.has_value());
    EXPECT_EQ(builder.getConfig().root->path, baseDir.path() / "merged");
    EXPECT_TRUE(builder.getConfig().root->readonly.value_or(false));
}

TEST_F(ContainerCfgBuilderTest, DisableUserNamespaceRemovesUserNs)
{
    ContainerCfgBuilder builder;
    builder.setAppId("org.deepin.demo")
      .setBasePath(baseDir.path())
      .setBundlePath(bundleDir.path())
      .disableUserNamespace();

    auto result = builder.build();
    ASSERT_TRUE(result.has_value()) << result.error().message();
    ASSERT_TRUE(builder.getConfig().linux_.has_value());
    ASSERT_TRUE(builder.getConfig().linux_->namespaces.has_value());
    for (const auto &ns : *builder.getConfig().linux_->namespaces) {
        EXPECT_NE(ns.type, ocppi::runtime::config::types::NamespaceType::User);
    }
}

TEST_F(ContainerCfgBuilderTest, IsolateNetworkAddsNetworkNamespace)
{
    ContainerCfgBuilder builder;
    builder.setAppId("org.deepin.demo")
      .setBasePath(baseDir.path())
      .setBundlePath(bundleDir.path())
      .isolateNetWork();

    auto result = builder.build();
    ASSERT_TRUE(result.has_value()) << result.error().message();
    ASSERT_TRUE(builder.getConfig().linux_.has_value());
    ASSERT_TRUE(builder.getConfig().linux_->namespaces.has_value());
    bool hasNetNs = false;
    for (const auto &ns : *builder.getConfig().linux_->namespaces) {
        if (ns.type == ocppi::runtime::config::types::NamespaceType::Network) {
            hasNetNs = true;
        }
    }
    EXPECT_TRUE(hasNetNs);
}

TEST_F(ContainerCfgBuilderTest, SetAnnotations)
{
    ContainerCfgBuilder builder;
    builder.setAppId("org.deepin.demo")
      .setBasePath(baseDir.path())
      .setBundlePath(bundleDir.path())
      .setAnnotation(ANNOTATION::APPID, "org.deepin.demo")
      .setAnnotation(ANNOTATION::BASEDIR, "/base")
      .setAnnotation(ANNOTATION::LAST_PID, "12345")
      .setAnnotation(ANNOTATION::WAYLAND_SOCKET, "/run/wayland-0")
      .setAnnotation(static_cast<ANNOTATION>(99), "unknown");

    auto result = builder.build();
    ASSERT_TRUE(result.has_value()) << result.error().message();
    ASSERT_TRUE(builder.getConfig().annotations.has_value());
    EXPECT_EQ(builder.getConfig().annotations->at("org.deepin.linglong.appID"), "org.deepin.demo");
    EXPECT_EQ(builder.getConfig().annotations->at("org.deepin.linglong.baseDir"), "/base");
    EXPECT_EQ(builder.getConfig().annotations->at("cn.org.linyaps.runtime.ns_last_pid"), "12345");
    EXPECT_EQ(builder.getConfig().annotations->at("cn.org.linyaps.runtime.ws.path"),
              "/run/wayland-0");
}

TEST_F(ContainerCfgBuilderTest, AddIdMappings)
{
    ContainerCfgBuilder builder;
    builder.setAppId("org.deepin.demo")
      .setBasePath(baseDir.path())
      .setBundlePath(bundleDir.path())
      .addUIdMapping(0, 1000, 1)
      .addUIdMapping(1, 1001, 1)
      .addGIdMapping(0, 2000, 1);

    auto result = builder.build();
    ASSERT_TRUE(result.has_value()) << result.error().message();
    ASSERT_TRUE(builder.getConfig().linux_.has_value());
    ASSERT_TRUE(builder.getConfig().linux_->uidMappings.has_value());
    ASSERT_EQ(builder.getConfig().linux_->uidMappings->size(), 2);
    EXPECT_EQ(builder.getConfig().linux_->uidMappings->at(0).containerID, 0);
    EXPECT_EQ(builder.getConfig().linux_->uidMappings->at(0).hostID, 1000);
    EXPECT_EQ(builder.getConfig().linux_->uidMappings->at(1).containerID, 1);
    ASSERT_TRUE(builder.getConfig().linux_->gidMappings.has_value());
    ASSERT_EQ(builder.getConfig().linux_->gidMappings->size(), 1);
    EXPECT_EQ(builder.getConfig().linux_->gidMappings->at(0).hostID, 2000);
}

TEST_F(ContainerCfgBuilderTest, SetCapabilities)
{
    ContainerCfgBuilder builder;
    builder.setAppId("org.deepin.demo")
      .setBasePath(baseDir.path())
      .setBundlePath(bundleDir.path())
      .setCapabilities({ "CAP_NET_ADMIN", "CAP_SYS_ADMIN" });

    auto result = builder.build();
    ASSERT_TRUE(result.has_value()) << result.error().message();
    ASSERT_TRUE(builder.getConfig().process.has_value());
    ASSERT_TRUE(builder.getConfig().process->args.has_value());
    EXPECT_EQ(builder.getConfig().process->args->at(0), "bash");
    ASSERT_TRUE(builder.getConfig().process->capabilities.has_value());
    EXPECT_EQ(builder.getConfig().process->capabilities->bounding,
              std::vector<std::string>({ "CAP_NET_ADMIN", "CAP_SYS_ADMIN" }));
    EXPECT_EQ(builder.getConfig().process->capabilities->effective,
              std::vector<std::string>({ "CAP_NET_ADMIN", "CAP_SYS_ADMIN" }));
}

TEST_F(ContainerCfgBuilderTest, AddMask)
{
    ContainerCfgBuilder builder;
    builder.setAppId("org.deepin.demo")
      .setBasePath(baseDir.path())
      .setBundlePath(bundleDir.path())
      .addMask({ "/proc/kcore", "/sys/firmware" });

    auto result = builder.build();
    ASSERT_TRUE(result.has_value()) << result.error().message();
    ASSERT_TRUE(builder.getConfig().linux_.has_value());
    ASSERT_TRUE(builder.getConfig().linux_->maskedPaths.has_value());
    EXPECT_EQ(builder.getConfig().linux_->maskedPaths->size(), 2);
    EXPECT_EQ(builder.getConfig().linux_->maskedPaths->at(0), "/proc/kcore");
}

TEST_F(ContainerCfgBuilderTest, AppPathProducesAppAndRuntimeMounts)
{
    ContainerCfgBuilder builder;
    builder.setAppId("org.deepin.demo")
      .setBasePath(baseDir.path())
      .setBundlePath(bundleDir.path())
      .setAppPath(appDir.path(), true)
      .setRuntimePath(runtimeDir.path(), false);

    auto result = builder.build();
    ASSERT_TRUE(result.has_value()) << result.error().message();

    ASSERT_TRUE(builder.getConfig().mounts.has_value());
    bool hasAppBind = false;
    bool hasRuntimeBind = false;
    for (const auto &m : *builder.getConfig().mounts) {
        if (m.destination == ContainerCfgBuilder::appMountPoint("org.deepin.demo")) {
            hasAppBind = true;
            EXPECT_EQ(m.source.value_or(""), appDir.path());
        }
        if (m.destination == ContainerCfgBuilder::runtimeMountPoint) {
            hasRuntimeBind = true;
            EXPECT_EQ(m.source.value_or(""), runtimeDir.path());
        }
    }
    EXPECT_TRUE(hasAppBind);
    EXPECT_TRUE(hasRuntimeBind);

    ASSERT_TRUE(builder.getConfig().process.has_value());
    ASSERT_TRUE(builder.getConfig().process->env.has_value());
    EXPECT_THAT(*builder.getConfig().process->env,
                ::testing::Contains("LINGLONG_APPID=org.deepin.demo"));
}

TEST_F(ContainerCfgBuilderTest, AppMountPointHelpers)
{
    EXPECT_EQ(ContainerCfgBuilder::appMountPoint("org.deepin.demo"),
              std::filesystem::path("/opt/apps/org.deepin.demo/files"));
    EXPECT_EQ(ContainerCfgBuilder::extensionMountPoint("org.deepin.ext"),
              std::filesystem::path("/opt/extensions/org.deepin.ext"));
    EXPECT_NE(ContainerCfgBuilder::runtimeMountPoint, std::filesystem::path{});
    EXPECT_NE(ContainerCfgBuilder::zoneinfoMountPoint, std::filesystem::path{});
}

TEST_F(ContainerCfgBuilderTest, ExtensionsAndExtraMountsHooks)
{
    Mount extra;
    extra.destination = "/opt/test";
    extra.source = "/host/test";
    extra.type = "bind";

    auto hook = ocppi::runtime::config::types::Hook{};
    hook.path = "/bin/true";

    ContainerCfgBuilder builder;
    builder.setAppId("org.deepin.demo")
      .setBasePath(baseDir.path())
      .setBundlePath(bundleDir.path())
      .addExtraMount(extra)
      .addExtraMounts({ extra })
      .setExtensionMounts({ extra })
      .setStartContainerHooks({ hook })
      .addExtraHook("prestart", hook);

    auto result = builder.build();
    ASSERT_TRUE(result.has_value()) << result.error().message();

    ASSERT_TRUE(builder.getConfig().mounts.has_value());
    bool hasExtra = false;
    for (const auto &m : *builder.getConfig().mounts) {
        if (m.destination == "/opt/test") {
            hasExtra = true;
        }
    }
    EXPECT_TRUE(hasExtra);

    ASSERT_TRUE(builder.getConfig().hooks.has_value());
    if (builder.getConfig().hooks->startContainer.has_value()) {
        EXPECT_EQ(builder.getConfig().hooks->startContainer->at(0).path, "/bin/true");
    }
}

TEST_F(ContainerCfgBuilderTest, ForwardAndAppendEnv)
{
    ContainerCfgBuilder builder;
    builder.setAppId("org.deepin.demo")
      .setBasePath(baseDir.path())
      .setBundlePath(bundleDir.path())
      .appendEnv("FOO", "bar")
      .appendEnv("QUOTED", "a'b")
      .forwardEnv({ "PATH" });

    auto result = builder.build();
    ASSERT_TRUE(result.has_value()) << result.error().message();

    ASSERT_TRUE(builder.getConfig().process.has_value());
    ASSERT_TRUE(builder.getConfig().process->env.has_value());
    const auto &env = *builder.getConfig().process->env;
    EXPECT_THAT(env, ::testing::Contains("FOO=bar"));
    EXPECT_THAT(env, ::testing::Contains("QUOTED=a'b"));

    // The env script must be generated inside the bundle, with single quotes
    // escaped as '\'' for the shell.
    EXPECT_TRUE(std::filesystem::exists(bundleDir.path() / "00env.sh"));
    std::ifstream envFile{ bundleDir.path() / "00env.sh" };
    ASSERT_TRUE(envFile.is_open());
    std::string content((std::istreambuf_iterator<char>(envFile)),
                        std::istreambuf_iterator<char>());
    EXPECT_THAT(content, ::testing::HasSubstr("export FOO='bar'"));
    EXPECT_THAT(content, ::testing::HasSubstr(R"(QUOTED='a'\''b')"));
}

TEST_F(ContainerCfgBuilderTest, EnableXDPWritesContainerInfo)
{
    ContainerCfgBuilder builder;
    builder.setAppId("org.deepin.demo")
      .setBasePath(baseDir.path())
      .setBundlePath(bundleDir.path())
      .setContainerId("ll-instance-1")
      .enableXDP(XdpOption{ .docMountPoint = "/usr/share/doc" });

    auto result = builder.build();
    ASSERT_TRUE(result.has_value()) << result.error().message();

    ASSERT_TRUE(builder.getConfig().process.has_value());
    ASSERT_TRUE(builder.getConfig().process->env.has_value());
    EXPECT_THAT(*builder.getConfig().process->env, ::testing::Contains("GTK_USE_PORTAL=1"));
    EXPECT_TRUE(std::filesystem::exists(bundleDir.path() / ".linyaps"));

    std::ifstream in{ bundleDir.path() / ".linyaps" };
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    EXPECT_THAT(content, ::testing::HasSubstr("org.deepin.demo"));
    EXPECT_THAT(content, ::testing::HasSubstr("ll-instance-1"));
}

TEST_F(ContainerCfgBuilderTest, BindHomeProducesHomeMounts)
{
    auto home = baseDir.path() / "home";
    ASSERT_TRUE(std::filesystem::create_directories(home));

    ContainerCfgBuilder builder;
    builder.setAppId("org.deepin.demo")
      .setBasePath(baseDir.path())
      .setBundlePath(bundleDir.path())
      .bindHome(home);

    auto result = builder.build();
    ASSERT_TRUE(result.has_value()) << result.error().message();

    ASSERT_TRUE(builder.getConfig().process.has_value());
    ASSERT_TRUE(builder.getConfig().process->env.has_value());

    std::vector<std::string> envHome;
    for (const auto &e : *builder.getConfig().process->env) {
        if (e.rfind("HOME=", 0) == 0) {
            envHome.push_back(e);
        }
    }
    EXPECT_FALSE(envHome.empty());
}

TEST_F(ContainerCfgBuilderTest, BindHomeWritesToEmptyHomePathFails)
{
    ContainerCfgBuilder builder;
    builder.setAppId("org.deepin.demo")
      .setBasePath(baseDir.path())
      .setBundlePath(bundleDir.path())
      .setAppPath(appDir.path())
      .bindHome("");

    // homePath empty -> error
    auto result = builder.build();
    ASSERT_FALSE(result.has_value());
}

TEST_F(ContainerCfgBuilderTest, EnablePrivateDirAndMapPrivate)
{
    auto home = baseDir.path() / "home";
    ASSERT_TRUE(std::filesystem::create_directories(home));

    ContainerCfgBuilder builder;
    builder.setAppId("org.deepin.demo")
      .setBasePath(baseDir.path())
      .setBundlePath(bundleDir.path())
      .bindHome(home)
      .enablePrivateDir()
      .mapPrivate("/var/lib/demo", true);

    auto result = builder.build();
    ASSERT_TRUE(result.has_value()) << result.error().message();

    // The private directory must be created inside the home and masked.
    auto privateAppDir = home / ".linglong" / "org.deepin.demo";
    EXPECT_TRUE(std::filesystem::exists(privateAppDir));

    ASSERT_TRUE(builder.getConfig().linux_.has_value());
    ASSERT_TRUE(builder.getConfig().linux_->maskedPaths.has_value());
    EXPECT_THAT(*builder.getConfig().linux_->maskedPaths, ::testing::Contains(home / ".linglong"));
}

TEST_F(ContainerCfgBuilderTest, MapPrivateRequiresPrivateDir)
{
    auto home = baseDir.path() / "home";
    ASSERT_TRUE(std::filesystem::create_directories(home));

    ContainerCfgBuilder builder;
    builder.setAppId("org.deepin.demo")
      .setBasePath(baseDir.path())
      .setBundlePath(bundleDir.path())
      .bindHome(home)
      .mapPrivate("/var/lib/demo", true);

    // privateMount not set -> "must enable private dir first"
    auto result = builder.build();
    ASSERT_FALSE(result.has_value());
}

TEST_F(ContainerCfgBuilderTest, BindUserGroupMountsPasswd)
{
    ContainerCfgBuilder builder;
    builder.setAppId("org.deepin.demo")
      .setBasePath(baseDir.path())
      .setBundlePath(bundleDir.path())
      .bindUserGroup();

    auto result = builder.build();
    ASSERT_TRUE(result.has_value()) << result.error().message();

    ASSERT_TRUE(builder.getConfig().mounts.has_value());
    bool hasPasswd = false;
    for (const auto &m : *builder.getConfig().mounts) {
        if (m.destination == "/etc/passwd") {
            hasPasswd = true;
        }
    }
    EXPECT_TRUE(hasPasswd);
}

TEST_F(ContainerCfgBuilderTest, BindDefaultMountsSysProcDevTmpRun)
{
    ContainerCfgBuilder builder;
    builder.setAppId("org.deepin.demo")
      .setBasePath(baseDir.path())
      .setBundlePath(bundleDir.path())
      .bindDefault();

    auto result = builder.build();
    ASSERT_TRUE(result.has_value()) << result.error().message();

    ASSERT_TRUE(builder.getConfig().mounts.has_value());
    const auto &mounts = *builder.getConfig().mounts;
    auto findDest = [&mounts](const std::string &dest) {
        for (const auto &m : mounts) {
            if (m.destination == dest) {
                return true;
            }
        }
        return false;
    };
    EXPECT_TRUE(findDest("/sys"));
    EXPECT_TRUE(findDest("/proc"));
    EXPECT_TRUE(findDest("/dev"));
    EXPECT_TRUE(findDest("/tmp"));
    EXPECT_TRUE(findDest("/run"));
}

TEST_F(ContainerCfgBuilderTest, BindDevPassthru)
{
    ContainerCfgBuilder builder;
    builder.setAppId("org.deepin.demo")
      .setBasePath(baseDir.path())
      .setBundlePath(bundleDir.path())
      .bindDev(true);

    auto result = builder.build();
    ASSERT_TRUE(result.has_value()) << result.error().message();
    ASSERT_TRUE(builder.getConfig().mounts.has_value());

    bool hasDevBind = false;
    for (const auto &m : *builder.getConfig().mounts) {
        if (m.destination == "/dev" && m.source.value_or("") == "/dev") {
            hasDevBind = true;
        }
    }
    EXPECT_TRUE(hasDevBind);
}

TEST_F(ContainerCfgBuilderTest, AppCacheAndLDCache)
{
    auto cache = baseDir.path() / "cache";
    ASSERT_TRUE(std::filesystem::create_directories(cache));

    ContainerCfgBuilder builder;
    builder.setAppId("org.deepin.demo")
      .setBasePath(baseDir.path())
      .setBundlePath(bundleDir.path())
      .setAppCache(cache)
      .enableLDConf()
      .enableLDCache();

    auto result = builder.build();
    ASSERT_TRUE(result.has_value()) << result.error().message();

    ASSERT_TRUE(builder.getConfig().mounts.has_value());
    bool hasCache = false;
    for (const auto &m : *builder.getConfig().mounts) {
        if (m.destination == "/run/linglong/cache") {
            hasCache = true;
            EXPECT_EQ(m.source.value_or(""), cache);
        }
    }
    EXPECT_TRUE(hasCache);
}

TEST_F(ContainerCfgBuilderTest, LDCacheRequiresAppCache)
{
    ContainerCfgBuilder builder;
    builder.setAppId("org.deepin.demo")
      .setBasePath(baseDir.path())
      .setBundlePath(bundleDir.path())
      .enableLDCache();

    auto result = builder.build();
    ASSERT_FALSE(result.has_value());
    EXPECT_THAT(result.error().message(), ::testing::HasSubstr("app cache is not set"));
}

TEST_F(ContainerCfgBuilderTest, XAuthAndWaylandSockets)
{
    auto authFile = baseDir.path() / "Xauthority";
    std::ofstream{ authFile } << "data";

    auto socket = baseDir.path() / "wayland-0";
    std::ofstream{ socket } << "data";

    ContainerCfgBuilder builder;
    builder.setAppId("org.deepin.demo")
      .setBasePath(baseDir.path())
      .setBundlePath(bundleDir.path())
      .bindRun()
      .bindXAuthFile(authFile)
      .bindWaylandSocket(socket);

    auto result = builder.build();
    ASSERT_TRUE(result.has_value()) << result.error().message();

    ASSERT_TRUE(builder.getConfig().process.has_value());
    ASSERT_TRUE(builder.getConfig().process->env.has_value());
    EXPECT_THAT(*builder.getConfig().process->env,
                ::testing::Contains("XAUTHORITY=/run/linglong/Xauthority"));
    EXPECT_THAT(*builder.getConfig().process->env,
                ::testing::Contains("WAYLAND_DISPLAY=/run/linglong/wayland-0"));
}

TEST_F(ContainerCfgBuilderTest, PipewireAndAtSpiSocketMounts)
{
    auto socket = baseDir.path() / "socket";
    std::ofstream{ socket } << "data";

    ContainerCfgBuilder builder;
    builder.setAppId("org.deepin.demo")
      .setBasePath(baseDir.path())
      .setBundlePath(bundleDir.path())
      .bindRun()
      .bindXDGRuntime()
      .enablePipewireSocketMount(PipewireMountOption{ .hostSocketPath = socket })
      .enableAtSpiSocketMount(AtSpiMountOption{ .hostSocketPath = socket });

    auto result = builder.build();
    ASSERT_TRUE(result.has_value()) << result.error().message();

    ASSERT_TRUE(builder.getConfig().mounts.has_value());
    const auto runtimeDir = std::filesystem::path{ "/run/user" } / std::to_string(::geteuid());
    bool hasPipewireMount = false;
    bool hasAtSpiMount = false;
    for (const auto &m : *builder.getConfig().mounts) {
        if (m.destination == runtimeDir / "pipewire-0") {
            hasPipewireMount = true;
            EXPECT_EQ(m.source.value_or(""), socket);
        }
        if (m.destination == runtimeDir / "at-spi" / "bus_0") {
            hasAtSpiMount = true;
        }
    }
    EXPECT_TRUE(hasPipewireMount);
    EXPECT_TRUE(hasAtSpiMount);
}

TEST_F(ContainerCfgBuilderTest, LdConfContainsAppAndRuntimePaths)
{
    ContainerCfgBuilder builder;
    builder.setAppId("org.deepin.demo").setRuntimePath(runtimeDir.path()).setAppPath(appDir.path());

    auto conf = builder.ldConf("x86_64-linux-gnu");
    EXPECT_THAT(conf, ::testing::HasSubstr("/opt/apps/org.deepin.demo/files/lib"));
    EXPECT_THAT(conf, ::testing::HasSubstr("/runtime/lib"));
    EXPECT_THAT(conf, ::testing::HasSubstr("x86_64-linux-gnu"));
}

TEST_F(ContainerCfgBuilderTest, TimezoneSetsBindArea)
{
    ContainerCfgBuilder builder;
    builder.setAppId("org.deepin.demo")
      .setBasePath(baseDir.path())
      .setBundlePath(bundleDir.path())
      .setTimezone("Asia/Shanghai");

    auto result = builder.build();
    ASSERT_TRUE(result.has_value()) << result.error().message();
    ASSERT_TRUE(builder.getConfig().mounts.has_value());
    EXPECT_TRUE(builder.getConfig().mounts->size() > 0);
}

TEST_F(ContainerCfgBuilderTest, ResolvConfWithoutHostRoot)
{
    auto resolv = baseDir.path() / "resolv.conf";
    std::ofstream{ resolv } << "nameserver 8.8.8.8\n";

    ContainerCfgBuilder builder;
    builder.setAppId("org.deepin.demo")
      .setBasePath(baseDir.path())
      .setBundlePath(bundleDir.path())
      .setResolvConf(resolv);

    auto result = builder.build();
    ASSERT_TRUE(result.has_value()) << result.error().message();
    ASSERT_TRUE(builder.getConfig().mounts.has_value());

    bool hasResolv = false;
    for (const auto &m : *builder.getConfig().mounts) {
        if (m.destination == "/etc/resolv.conf") {
            hasResolv = true;
            EXPECT_EQ(m.source.value_or(""), resolv);
        }
    }
    EXPECT_TRUE(hasResolv);
}

} // namespace
