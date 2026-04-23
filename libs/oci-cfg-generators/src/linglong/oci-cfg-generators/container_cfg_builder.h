/*
 * SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "linglong/cdi/types/ContainerEdits.hpp"
#include "linglong/utils/error/error.h"
#include "ocppi/runtime/config/types/Config.hpp"
#include "ocppi/runtime/config/types/IdMapping.hpp"
#include "ocppi/runtime/config/types/Mount.hpp"

#include <filesystem>
#include <unordered_map>
#include <unordered_set>

namespace linglong::generator {

enum class ANNOTATION {
    APPID,
    BASEDIR,
    LAST_PID,
    WAYLAND_SOCKET,
};

class ContainerCfgBuilder
{
public:
    struct MountNode
    {
        std::string name;
        bool ro;
        int mount_idx;
        std::vector<int> childs_idx;
        int parent_idx;
    };

    inline static const std::filesystem::path runtimeMountPoint{ "/runtime" };
    inline static const std::filesystem::path zoneinfoMountPoint{ "/usr/share/zoneinfo" };

    ContainerCfgBuilder &setAppId(const std::string &id) noexcept
    {
        appId = id;
        return *this;
    }

    ContainerCfgBuilder &setAppPath(std::filesystem::path path, bool isRo = true) noexcept
    {
        appPath = path;
        appPathRo = isRo;
        return *this;
    }

    ContainerCfgBuilder &setRuntimePath(std::filesystem::path path, bool isRo = true) noexcept
    {
        runtimePath = path;
        runtimePathRo = isRo;
        return *this;
    }

    ContainerCfgBuilder &setBasePath(const std::filesystem::path &path, bool isRo = true) noexcept
    {
        basePath = path;
        basePathRo = isRo;
        return *this;
    }

    ContainerCfgBuilder &setBundlePath(const std::filesystem::path &path) noexcept
    {
        bundlePath = path;
        return *this;
    }

    ContainerCfgBuilder &setAppCache(std::filesystem::path path, bool isRo = true) noexcept
    {
        appCache = path;
        appCacheRo = isRo;
        return *this;
    }

    ContainerCfgBuilder &setTimezone(std::string value) noexcept
    {
        timezone = std::move(value);
        return *this;
    }

    ContainerCfgBuilder &setAnnotation(ANNOTATION annotation, std::string value) noexcept;

    ContainerCfgBuilder &addUIdMapping(int64_t containerID, int64_t hostID, int64_t size) noexcept;
    ContainerCfgBuilder &addGIdMapping(int64_t containerID, int64_t hostID, int64_t size) noexcept;

    ContainerCfgBuilder &bindDefault() noexcept;
    ContainerCfgBuilder &bindSys() noexcept;
    ContainerCfgBuilder &bindProc() noexcept;
    ContainerCfgBuilder &bindDev() noexcept;
    ContainerCfgBuilder &
    bindDevNode(std::function<bool(const std::string &)> ifBind = nullptr) noexcept;
    ContainerCfgBuilder &bindCgroup() noexcept;
    ContainerCfgBuilder &bindXDGRuntime() noexcept;
    ContainerCfgBuilder &bindRun() noexcept;
    ContainerCfgBuilder &bindTmp() noexcept;
    ContainerCfgBuilder &bindUserGroup() noexcept;
    ContainerCfgBuilder &bindRemovableStorageMounts() noexcept;

    ContainerCfgBuilder &forwardDefaultEnv() noexcept;
    ContainerCfgBuilder &forwardEnv(const std::vector<std::string> &envList = {}) noexcept;
    ContainerCfgBuilder &appendEnv(const std::map<std::string, std::string> &envMap) noexcept;
    ContainerCfgBuilder &appendEnv(const std::string &env,
                                   const std::string &value,
                                   bool overwrite = false) noexcept;

    ContainerCfgBuilder &bindHostRoot() noexcept;
    ContainerCfgBuilder &bindHostStatics() noexcept;
    ContainerCfgBuilder &bindHome(std::filesystem::path hostHome) noexcept;

    ContainerCfgBuilder &bindXAuthFile(const std::filesystem::path &authFile) noexcept;
    ContainerCfgBuilder &bindWaylandSocket(const std::filesystem::path &socket) noexcept;

    ContainerCfgBuilder &enablePrivateDir() noexcept;
    ContainerCfgBuilder &mapPrivate(std::string containerPath, bool isDir) noexcept;
    ContainerCfgBuilder &bindIPC() noexcept;
    ContainerCfgBuilder &enableLDConf() noexcept;
    ContainerCfgBuilder &enableLDCache() noexcept;

    ContainerCfgBuilder &setContainerId(std::string containerId) noexcept;

    const std::string &getContainerId() const noexcept { return containerId; }

    // TODO
    ContainerCfgBuilder &enableFontCache() noexcept { return *this; }

    ContainerCfgBuilder &enableQuirkVolatile() noexcept;

    ContainerCfgBuilder &disableContainerInfo() noexcept;

    ContainerCfgBuilder &
      setExtensionMounts(std::vector<ocppi::runtime::config::types::Mount>) noexcept;
    ContainerCfgBuilder &addExtraMount(ocppi::runtime::config::types::Mount) noexcept;
    ContainerCfgBuilder &addExtraMounts(std::vector<ocppi::runtime::config::types::Mount>) noexcept;

    ContainerCfgBuilder &
      setStartContainerHooks(std::vector<ocppi::runtime::config::types::Hook>) noexcept;

    ContainerCfgBuilder &addExtraHook(const std::string &type,
                                      ocppi::runtime::config::types::Hook hook) noexcept;

    ContainerCfgBuilder &enableSelfAdjustingMount() noexcept
    {
        selfAdjustingMountEnabled = true;
        return *this;
    }

    ContainerCfgBuilder &enableOverlayMode(std::filesystem::path merged, bool readOnly) noexcept
    {
        overlayMerged = std::make_pair(std::move(merged), readOnly);
        return *this;
    }

    ContainerCfgBuilder &addMask(const std::vector<std::string> &masks) noexcept;

    ContainerCfgBuilder &isolateNetWork() noexcept
    {
        isolateNetWorkEnabled = true;
        return *this;
    }

    ContainerCfgBuilder &disableUserNamespace() noexcept
    {
        disableUserNamespaceEnabled = true;
        return *this;
    }

    ContainerCfgBuilder &disablePatch() noexcept
    {
        applyPatchEnabled = false;
        return *this;
    }

    ContainerCfgBuilder &setCapabilities(std::vector<std::string> caps) noexcept
    {
        capabilities = std::move(caps);
        return *this;
    }

    utils::error::Result<void>
    applyCDIPatch(const linglong::cdi::types::ContainerEdits &edits) noexcept;

    std::string ldConf(const std::string &triplet) const;

    utils::error::Result<void> build() noexcept;

    const ocppi::runtime::config::types::Config &getConfig() const { return config; }

private:
    utils::error::Result<void> checkValid() noexcept;
    utils::error::Result<void> prepare() noexcept;
    utils::error::Result<void> buildIdMappings() noexcept;
    utils::error::Result<void> buildMountRuntime() noexcept;
    utils::error::Result<void> buildMountApp() noexcept;
    utils::error::Result<void> buildMountHome() noexcept;
    utils::error::Result<void> buildPrivateDir() noexcept;
    utils::error::Result<void> buildPrivateMapped() noexcept;
    utils::error::Result<void> buildMountIPC() noexcept;
    utils::error::Result<void> buildDisplaySystem() noexcept;
    utils::error::Result<void> buildMountCache() noexcept;
    utils::error::Result<void> buildLDCache() noexcept;
    utils::error::Result<void> buildMountTimeZone() noexcept;
    utils::error::Result<void> buildMountNetworkConf() noexcept;
    utils::error::Result<void> buildQuirkVolatile() noexcept;
    utils::error::Result<void> buildXDGRuntime() noexcept;
    utils::error::Result<void> buildEnv() noexcept;
    utils::error::Result<void> buildContainerInfo() noexcept;
    utils::error::Result<void> buildHooks() noexcept;
    utils::error::Result<void> applyPatch() noexcept;
    utils::error::Result<void> applyPatchFile(const std::filesystem::path &patchFile) noexcept;
    utils::error::Result<void> applyJsonPatchFile(const std::filesystem::path &patchFile) noexcept;
    utils::error::Result<void>
    applyExecutablePatch(const std::filesystem::path &patchFile) noexcept;
    utils::error::Result<void> mergeMount() noexcept;
    utils::error::Result<void> finalize() noexcept;

    // utility functions
    static utils::error::Result<void>
    mountBind(const ocppi::runtime::config::types::Mount &mount) noexcept;

    // adjust mount
    int findChild(int parent, const std::string &name) noexcept;
    int insertChild(int parent, MountNode node) noexcept;
    int insertChildRecursively(const std::filesystem::path &path, bool &inserted) noexcept;
    int findNearestMountNode(int child) noexcept;
    bool shouldFix(int node, std::filesystem::path &fixPath) noexcept;
    std::string getRelativePath(int parent, int node) noexcept;
    void adjustNode(int node,
                    const std::filesystem::path &path,
                    const std::filesystem::path fixPath) noexcept;
    utils::error::Result<void> constructMountpointsTree() noexcept;
    void tryFixMountpointsTree() noexcept;
    void generateMounts() noexcept;
    utils::error::Result<void> selfAdjustingMount() noexcept;

    // path settings
    std::string appId;
    std::optional<std::filesystem::path> runtimePath;
    std::optional<std::filesystem::path> appPath;
    std::filesystem::path basePath;
    std::filesystem::path bundlePath;
    std::optional<std::filesystem::path> appCache;
    std::optional<std::filesystem::path> containerXDGRuntimeDir;
    std::optional<std::string> timezone;

    bool runtimePathRo = true;
    bool appPathRo = true;
    bool basePathRo = true;
    bool appCacheRo = true;

    // id mappings
    std::optional<std::vector<ocppi::runtime::config::types::IdMapping>> uidMappings;
    std::optional<std::vector<ocppi::runtime::config::types::IdMapping>> gidMappings;

    // mount
    std::optional<ocppi::runtime::config::types::Mount> infoMount;
    std::optional<ocppi::runtime::config::types::Mount> runtimeMount;
    std::optional<std::vector<ocppi::runtime::config::types::Mount>> appMount;
    std::optional<ocppi::runtime::config::types::Mount> sysMount;
    std::optional<ocppi::runtime::config::types::Mount> procMount;
    std::optional<std::vector<ocppi::runtime::config::types::Mount>> devMount;
    std::optional<std::vector<ocppi::runtime::config::types::Mount>> devNodeMount;
    std::optional<ocppi::runtime::config::types::Mount> cgroupMount;
    std::optional<std::vector<ocppi::runtime::config::types::Mount>> runMount;
    std::optional<ocppi::runtime::config::types::Mount> tmpMount;
    std::optional<std::vector<ocppi::runtime::config::types::Mount>> UGMount;
    std::optional<std::vector<ocppi::runtime::config::types::Mount>> removableStorageMounts;
    std::optional<std::vector<ocppi::runtime::config::types::Mount>> hostRootMount;
    std::optional<std::vector<ocppi::runtime::config::types::Mount>> hostStaticsMount;
    std::optional<std::vector<ocppi::runtime::config::types::Mount>> ipcMount;
    std::optional<std::vector<ocppi::runtime::config::types::Mount>> displayMount;
    std::optional<std::vector<ocppi::runtime::config::types::Mount>> timeZoneMount;
    std::optional<std::vector<ocppi::runtime::config::types::Mount>> networkConfMount;

    // cache
    std::optional<std::vector<ocppi::runtime::config::types::Mount>> cacheMount;
    std::optional<std::vector<ocppi::runtime::config::types::Mount>> ldCacheMount;
    std::optional<std::vector<ocppi::runtime::config::types::Mount>> ldConfMount;

    // environment
    std::unordered_set<std::string> envForward;
    std::unordered_map<std::string, std::string> environment;
    std::unordered_map<std::string, std::string> envAppend;
    std::optional<ocppi::runtime::config::types::Mount> envMount;

    // home dir
    std::optional<std::filesystem::path> homePath;
    std::optional<std::vector<ocppi::runtime::config::types::Mount>> homeMount;

    // private dir
    std::filesystem::path privatePath;
    std::filesystem::path privateAppDir;
    std::optional<std::vector<ocppi::runtime::config::types::Mount>> privateMount;
    std::optional<std::map<std::string, bool>> privateMappings;

    // volatile
    std::optional<std::vector<ocppi::runtime::config::types::Mount>> volatileMount;

    // extension mounts
    std::optional<std::vector<ocppi::runtime::config::types::Mount>> extensionMount;
    // extra mounts
    std::optional<std::vector<ocppi::runtime::config::types::Mount>> extraMount;

    std::optional<std::vector<ocppi::runtime::config::types::Hook>> startContainerHooks;
    std::optional<std::vector<std::pair<std::string, ocppi::runtime::config::types::Hook>>>
      extraHooks;

    // self-adjusting mount
    bool selfAdjustingMountEnabled = false;
    // mountpoints is a prefix tree of all mounts path
    // .mount_idx > 0 represents the path is a mount point, and it's the subscript of the array
    // mounts
    std::vector<MountNode> mountpoints;
    // this 'mounts' is used internally, distinct from config.mounts
    std::vector<ocppi::runtime::config::types::Mount> mounts;

    std::optional<std::pair<std::filesystem::path, bool>> overlayMerged;

    bool isolateNetWorkEnabled = false;
    bool disableUserNamespaceEnabled = false;
    bool disableGenerateContainerInfo{ true };
    bool applyPatchEnabled = true;
    bool isolateTmp{ false };

    // display system
    std::optional<std::filesystem::path> waylandSocket;
    std::optional<std::filesystem::path> xAuthFile;

    std::vector<std::string> maskedPaths;
    std::optional<std::vector<std::string>> capabilities;
    ocppi::runtime::config::types::Config config;
    std::string containerId;
};

}; // namespace linglong::generator
