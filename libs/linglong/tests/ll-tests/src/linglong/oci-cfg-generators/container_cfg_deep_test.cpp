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

namespace linglong::generator::test {

namespace fs = std::filesystem;

class ContainerCfgBuilderDeepSuite : public ::testing::Test
{
protected:
    void SetUp() override
    {
        ASSERT_TRUE(baseDir.isValid());
        ASSERT_TRUE(bundleDir.isValid());
        ASSERT_TRUE(appDir.isValid());
        ASSERT_TRUE(runtimeDir.isValid());
    }

    TempDir baseDir{ "ll-cfg-deep-base-" };
    TempDir bundleDir{ "ll-cfg-deep-bundle-" };
    TempDir appDir{ "ll-cfg-deep-app-" };
    TempDir runtimeDir{ "ll-cfg-deep-runtime-" };
};

TEST_F(ContainerCfgBuilderDeepSuite, EnvironmentVariablesDeduplicationAndOverwrite)
{
    ContainerCfgBuilder builder;
    builder.setAppId("org.deepin.testapp")
      .setBasePath(baseDir.path())
      .setBundlePath(bundleDir.path())
      .setContainerId("env-test-id")
      .disablePatch();

    builder.appendEnv("LINGLONG_CUSTOM_KEY", "initial_val", false);
    builder.appendEnv("LINGLONG_CUSTOM_KEY", "overwritten_val", true);
    builder.appendEnv("LINGLONG_PRESERVED_KEY", "fixed_val", false);
    builder.appendEnv("LINGLONG_PRESERVED_KEY", "ignored_val", false); // overwrite=false

    builder.appendEnv(std::map<std::string, std::string>{
      { "MAP_KEY_A", "val_a" },
      { "MAP_KEY_B", "val_b" },
    });

    auto res = builder.build();
    ASSERT_TRUE(res.has_value());

    const auto &cfg = builder.getConfig();
    ASSERT_TRUE(cfg.process.has_value());
    ASSERT_TRUE(cfg.process->env.has_value());

    const auto &envList = *cfg.process->env;

    auto findEnv = [&](const std::string &prefix) -> std::optional<std::string> {
        for (const auto &entry : envList) {
            if (entry.rfind(prefix, 0) == 0) {
                return entry;
            }
        }
        return std::nullopt;
    };

    auto customEntry = findEnv("LINGLONG_CUSTOM_KEY=");
    ASSERT_TRUE(customEntry.has_value());
    EXPECT_EQ(*customEntry, "LINGLONG_CUSTOM_KEY=overwritten_val");

    auto preservedEntry = findEnv("LINGLONG_PRESERVED_KEY=");
    ASSERT_TRUE(preservedEntry.has_value());
    EXPECT_EQ(*preservedEntry, "LINGLONG_PRESERVED_KEY=fixed_val");

    auto mapEntryA = findEnv("MAP_KEY_A=");
    ASSERT_TRUE(mapEntryA.has_value());
    EXPECT_EQ(*mapEntryA, "MAP_KEY_A=val_a");
}

TEST_F(ContainerCfgBuilderDeepSuite, IdMappingMultipleUsersAndGroups)
{
    ContainerCfgBuilder builder;
    builder.setAppId("org.deepin.idmapping")
      .setBasePath(baseDir.path())
      .setBundlePath(bundleDir.path())
      .setContainerId("idmap-test-id")
      .disablePatch();

    builder.addUIdMapping(0, 1000, 1);
    builder.addUIdMapping(1, 100000, 65536);
    builder.addGIdMapping(0, 1000, 1);
    builder.addGIdMapping(1, 100000, 65536);

    auto res = builder.build();
    ASSERT_TRUE(res.has_value());

    const auto &cfg = builder.getConfig();
    ASSERT_TRUE(cfg.linux_.has_value());
    ASSERT_TRUE(cfg.linux_->uidMappings.has_value());
    ASSERT_TRUE(cfg.linux_->gidMappings.has_value());

    const auto &uids = *cfg.linux_->uidMappings;
    ASSERT_GE(uids.size(), 2);
    EXPECT_EQ(uids[0].containerID, 0);
    EXPECT_EQ(uids[0].hostID, 1000);
    EXPECT_EQ(uids[0].size, 1);

    const auto &gids = *cfg.linux_->gidMappings;
    ASSERT_GE(gids.size(), 2);
    EXPECT_EQ(gids[0].containerID, 0);
    EXPECT_EQ(gids[0].hostID, 1000);
    EXPECT_EQ(gids[0].size, 1);
}

TEST_F(ContainerCfgBuilderDeepSuite, ExtraMountsAndOrderingIntegrity)
{
    ContainerCfgBuilder builder;
    builder.setAppId("org.deepin.mounttest")
      .setBasePath(baseDir.path())
      .setBundlePath(bundleDir.path())
      .setContainerId("mounts-test-id")
      .disablePatch();

    Mount m1;
    m1.destination = "/opt/custom_lib1";
    m1.source = "/usr/lib/custom1";
    m1.type = "bind";
    m1.options = { "ro", "rbind" };

    Mount m2;
    m2.destination = "/opt/custom_lib2";
    m2.source = "/usr/lib/custom2";
    m2.type = "bind";
    m2.options = { "rw", "rbind" };

    builder.addExtraMount(m1);
    builder.addExtraMounts({ m2 });

    auto res = builder.build();
    ASSERT_TRUE(res.has_value());

    const auto &cfg = builder.getConfig();
    ASSERT_TRUE(cfg.mounts.has_value());

    bool foundM1 = false;
    bool foundM2 = false;
    for (const auto &m : *cfg.mounts) {
        if (m.destination == "/opt/custom_lib1") {
            foundM1 = true;
            EXPECT_EQ(m.source, "/usr/lib/custom1");
        }
        if (m.destination == "/opt/custom_lib2") {
            foundM2 = true;
            EXPECT_EQ(m.source, "/usr/lib/custom2");
        }
    }
    EXPECT_TRUE(foundM1);
    EXPECT_TRUE(foundM2);
}

TEST_F(ContainerCfgBuilderDeepSuite, TimezoneAndResolvConfMounts)
{
    auto fakeResolv = tempDir.path() / "resolv.conf";
    {
        std::ofstream ofs(fakeResolv);
        ofs << "nameserver 127.0.0.53\n";
    }

    ContainerCfgBuilder builder;
    builder.setAppId("org.deepin.networkapp")
      .setBasePath(baseDir.path())
      .setBundlePath(bundleDir.path())
      .setContainerId("tz-resolv-test")
      .setTimezone("Asia/Shanghai")
      .setResolvConf(fakeResolv)
      .disablePatch();

    auto res = builder.build();
    ASSERT_TRUE(res.has_value());

    const auto &cfg = builder.getConfig();
    ASSERT_TRUE(cfg.process.has_value());
    ASSERT_TRUE(cfg.process->env.has_value());

    bool hasTzEnv = false;
    for (const auto &envStr : *cfg.process->env) {
        if (envStr == "TZ=Asia/Shanghai") {
            hasTzEnv = true;
        }
    }
    EXPECT_TRUE(hasTzEnv);
}

TEST_F(ContainerCfgBuilderDeepSuite, OverlayModeConfiguration)
{
    auto fakeMerged = tempDir.path() / "overlay_merged";
    fs::create_directories(fakeMerged);

    ContainerCfgBuilder builder;
    builder.setAppId("org.deepin.overlayapp")
      .setBasePath(baseDir.path())
      .setBundlePath(bundleDir.path())
      .setContainerId("overlay-test-id")
      .enableOverlayMode(fakeMerged, true)
      .disablePatch();

    auto res = builder.build();
    ASSERT_TRUE(res.has_value());

    const auto &cfg = builder.getConfig();
    ASSERT_TRUE(cfg.root.has_value());
    EXPECT_TRUE(cfg.root->readonly.value_or(false));
}

TEST_F(ContainerCfgBuilderDeepSuite, AnnotationsAssignment)
{
    ContainerCfgBuilder builder;
    builder.setAppId("org.deepin.annottest")
      .setBasePath(baseDir.path())
      .setBundlePath(bundleDir.path())
      .setContainerId("annot-test-id")
      .setAnnotation(ANNOTATION::APPID, "org.deepin.annottest")
      .setAnnotation(ANNOTATION::WAYLAND_SOCKET, "wayland-0")
      .setAnnotation(ANNOTATION::LAST_PID, "12345")
      .disablePatch();

    auto res = builder.build();
    ASSERT_TRUE(res.has_value());

    const auto &cfg = builder.getConfig();
    ASSERT_TRUE(cfg.annotations.has_value());
    const auto &annots = *cfg.annotations;

    EXPECT_NE(annots.find("org.deepin.linglong.appId"), annots.end());
    EXPECT_EQ(annots.at("org.deepin.linglong.appId"), "org.deepin.annottest");
}

TEST_F(ContainerCfgBuilderDeepSuite, PipewireAndAtSpiOptions)
{
    auto pwSocket = tempDir.path() / "pipewire-0";
    auto atSpiSocket = tempDir.path() / "at-spi-bus";
    {
        std::ofstream(pwSocket) << "";
        std::ofstream(atSpiSocket) << "";
    }

    ContainerCfgBuilder builder;
    builder.setAppId("org.deepin.media")
      .setBasePath(baseDir.path())
      .setBundlePath(bundleDir.path())
      .setContainerId("media-socket-test")
      .enablePipewireSocketMount(PipewireMountOption{ .hostSocketPath = pwSocket })
      .enableAtSpiSocketMount(AtSpiMountOption{ .hostSocketPath = atSpiSocket })
      .disablePatch();

    auto res = builder.build();
    ASSERT_TRUE(res.has_value());
}

TEST_F(ContainerCfgBuilderDeepSuite, SecurityCapabilitiesBoundingAndEffective)
{
    ContainerCfgBuilder builder;
    builder.setAppId("org.deepin.seccap")
      .setBasePath(baseDir.path())
      .setBundlePath(bundleDir.path())
      .setContainerId("caps-test-id")
      .disablePatch();

    auto res = builder.build();
    ASSERT_TRUE(res.has_value());

    const auto &cfg = builder.getConfig();
    ASSERT_TRUE(cfg.process.has_value());
    ASSERT_TRUE(cfg.process->capabilities.has_value());

    const auto &caps = *cfg.process->capabilities;
    EXPECT_FALSE(caps.bounding.empty());
    EXPECT_FALSE(caps.effective.empty());
    EXPECT_FALSE(caps.permitted.empty());
}

TEST_F(ContainerCfgBuilderDeepSuite, ReadonlyPathsAndMaskedPathsProtection)
{
    ContainerCfgBuilder builder;
    builder.setAppId("org.deepin.sandbox")
      .setBasePath(baseDir.path())
      .setBundlePath(bundleDir.path())
      .setContainerId("sandbox-test-id")
      .disablePatch();

    auto res = builder.build();
    ASSERT_TRUE(res.has_value());

    const auto &cfg = builder.getConfig();
    ASSERT_TRUE(cfg.linux_.has_value());
    ASSERT_TRUE(cfg.linux_->maskedPaths.has_value());
    ASSERT_TRUE(cfg.linux_->readonlyPaths.has_value());

    const auto &masked = *cfg.linux_->maskedPaths;
    EXPECT_FALSE(masked.empty());

    const auto &roPaths = *cfg.linux_->readonlyPaths;
    EXPECT_FALSE(roPaths.empty());
}

TEST_F(ContainerCfgBuilderDeepSuite, BindDevNodesAndPassthroughModes)
{
    ContainerCfgBuilder builder;
    builder.setAppId("org.deepin.devnode")
      .setBasePath(baseDir.path())
      .setBundlePath(bundleDir.path())
      .setContainerId("devnode-test-id")
      .bindDev(true) // passthru
      .bindDevNode([](const std::string &devPath) {
          return devPath.find("null") != std::string::npos
            || devPath.find("zero") != std::string::npos;
      })
      .disablePatch();

    auto res = builder.build();
    ASSERT_TRUE(res.has_value());

    const auto &cfg = builder.getConfig();
    ASSERT_TRUE(cfg.mounts.has_value());

    bool hasDev = false;
    for (const auto &m : *cfg.mounts) {
        if (m.destination == "/dev") {
            hasDev = true;
            break;
        }
    }
    EXPECT_TRUE(hasDev);
}

TEST_F(ContainerCfgBuilderDeepSuite, BindHostStaticsAndRootHierarchy)
{
    ContainerCfgBuilder builder;
    builder.setAppId("org.deepin.hoststat")
      .setBasePath(baseDir.path())
      .setBundlePath(bundleDir.path())
      .setContainerId("hoststat-test-id")
      .bindHostStatics()
      .disablePatch();

    auto res = builder.build();
    ASSERT_TRUE(res.has_value());

    const auto &cfg = builder.getConfig();
    ASSERT_TRUE(cfg.mounts.has_value());
    EXPECT_FALSE(cfg.mounts->empty());
}

TEST_F(ContainerCfgBuilderDeepSuite, XAuthAndWaylandSocketBinds)
{
    auto fakeXauth = tempDir.path() / ".Xauthority";
    auto fakeWayland = tempDir.path() / "wayland-0";
    {
        std::ofstream(fakeXauth) << "auth_secret";
        std::ofstream(fakeWayland) << "";
    }

    ContainerCfgBuilder builder;
    builder.setAppId("org.deepin.graphics")
      .setBasePath(baseDir.path())
      .setBundlePath(bundleDir.path())
      .setContainerId("graphics-test-id")
      .bindXAuthFile(fakeXauth)
      .bindWaylandSocket(fakeWayland)
      .disablePatch();

    auto res = builder.build();
    ASSERT_TRUE(res.has_value());

    const auto &cfg = builder.getConfig();
    ASSERT_TRUE(cfg.mounts.has_value());

    bool foundXauth = false;
    bool foundWayland = false;
    for (const auto &m : *cfg.mounts) {
        if (m.destination.rfind("/tmp/", 0) == 0 && m.source == fakeXauth) {
            foundXauth = true;
        }
        if (m.source == fakeWayland) {
            foundWayland = true;
        }
    }
    EXPECT_TRUE(foundXauth);
    EXPECT_TRUE(foundWayland);
}

TEST_F(ContainerCfgBuilderDeepSuite, PrivateDirectoryAndFileMappings)
{
    ContainerCfgBuilder builder;
    builder.setAppId("org.deepin.private")
      .setBasePath(baseDir.path())
      .setBundlePath(bundleDir.path())
      .setContainerId("private-test-id")
      .enablePrivateDir()
      .mapPrivate("/etc/private_config.conf", false)
      .mapPrivate("/var/private_cache", true)
      .disablePatch();

    auto res = builder.build();
    ASSERT_TRUE(res.has_value());

    const auto &cfg = builder.getConfig();
    ASSERT_TRUE(cfg.mounts.has_value());
    EXPECT_FALSE(cfg.mounts->empty());
}

TEST_F(ContainerCfgBuilderDeepSuite, BindUserGroupMinimalAndFull)
{
    ContainerCfgBuilder builder;
    builder.setAppId("org.deepin.usergrp")
      .setBasePath(baseDir.path())
      .setBundlePath(bundleDir.path())
      .setContainerId("usergrp-test-id")
      .bindUserGroup(true)
      .disablePatch();

    auto res = builder.build();
    ASSERT_TRUE(res.has_value());

    const auto &cfg = builder.getConfig();
    ASSERT_TRUE(cfg.mounts.has_value());

    bool foundPasswd = false;
    for (const auto &m : *cfg.mounts) {
        if (m.destination == "/etc/passwd") {
            foundPasswd = true;
            break;
        }
    }
    EXPECT_TRUE(foundPasswd);
}

TEST_F(ContainerCfgBuilderDeepSuite, MountPointValidationAndOrder)
{
    ContainerCfgBuilder builder;
    builder.setAppId("org.deepin.order")
      .setBasePath(baseDir.path())
      .setBundlePath(bundleDir.path())
      .setContainerId("order-test-id")
      .bindProc()
      .bindSys()
      .bindTmp()
      .bindRun()
      .bindCgroup()
      .disablePatch();

    auto res = builder.build();
    ASSERT_TRUE(res.has_value());

    const auto &cfg = builder.getConfig();
    ASSERT_TRUE(cfg.mounts.has_value());

    std::unordered_set<std::string> destinations;
    for (const auto &m : *cfg.mounts) {
        destinations.insert(m.destination);
    }
    EXPECT_TRUE(destinations.count("/proc"));
    EXPECT_TRUE(destinations.count("/sys"));
    EXPECT_TRUE(destinations.count("/tmp"));
    EXPECT_TRUE(destinations.count("/run"));
}

TEST_F(ContainerCfgBuilderDeepSuite, ExtensionMountsAndCdiIntegration)
{
    ContainerCfgBuilder builder;
    builder.setAppId("org.deepin.extapp")
      .setBasePath(baseDir.path())
      .setBundlePath(bundleDir.path())
      .setContainerId("extapp-test-id")
      .disablePatch();

    Mount extMount;
    extMount.destination = "/opt/extensions/plugin1";
    extMount.source = "/usr/lib/ext/plugin1";
    extMount.type = "bind";
    extMount.options = { "ro", "nosuid", "nodev" };

    builder.setExtensionMounts({ extMount });
    auto res = builder.build();
    ASSERT_TRUE(res.has_value());

    const auto &cfg = builder.getConfig();
    ASSERT_TRUE(cfg.mounts.has_value());

    bool foundExt = false;
    for (const auto &m : *cfg.mounts) {
        if (m.destination == "/opt/extensions/plugin1") {
            foundExt = true;
            EXPECT_EQ(m.source, "/usr/lib/ext/plugin1");
            break;
        }
    }
    EXPECT_TRUE(foundExt);
}

TEST_F(ContainerCfgBuilderDeepSuite, SelfAdjustingMountFlagSwitch)
{
    ContainerCfgBuilder builder;
    builder.setAppId("org.deepin.selfadj")
      .setBasePath(baseDir.path())
      .setBundlePath(bundleDir.path())
      .setContainerId("selfadj-test-id")
      .enableSelfAdjustingMount()
      .disablePatch();

    auto res = builder.build();
    ASSERT_TRUE(res.has_value());
}

} // namespace linglong::generator::test
