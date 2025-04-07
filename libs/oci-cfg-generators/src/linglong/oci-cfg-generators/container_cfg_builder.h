/*
 * SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "ocppi/runtime/config/types/Config.hpp"
#include "ocppi/runtime/config/types/IdMapping.hpp"
#include "ocppi/runtime/config/types/Mount.hpp"

#include <filesystem>
#include <map>

namespace linglong::generator {

enum class ANNOTATION {
    ANNOTATION_APPID,
    ANNOTATION_BASEDIR,
};

class ContainerCfgBuilder
{
public:
    enum ERROR_CODE {
        BUILD_SUCCESS,
        BUILD_PARAM_ERROR,
        BUILD_PREPARE_ERROR,
        BUILD_MOUNT_RUNTIME_ERROR,
        BUILD_MOUNT_APP_ERROR,
        BUILD_MOUNT_HOME_ERROR,
        BUILD_MOUNT_TMP_ERROR,
        BUILD_PRIVATEDIR_ERROR,
        BUILD_PRIVATEMAP_ERROR,
        BUILD_MOUNT_IPC_ERROR,
        BUILD_MOUNT_CACHE_ERROR,
        BUILD_MOUNT_VOLATILE_ERROR,
        BUILD_MOUNT_ERROR,
        BUILD_LDCONF_ERROR,
        BUILD_LDCACHE_ERROR,
        BUILD_ENV_ERROR,
    };

    class Error
    {
    public:
        operator bool() const { return code != BUILD_SUCCESS; }

        ERROR_CODE code;
        std::string reason;
    };

    struct MountNode
    {
        std::string name;
        bool ro;
        bool fix;
        int mount_idx;
        std::vector<int> childs_idx;
        int parent_idx;
    };

    ContainerCfgBuilder &setAppId(std::string id) noexcept
    {
        appId = id;
        return *this;
    }

    std::string getAppId() const { return appId; }

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

    ContainerCfgBuilder &setBasePath(std::filesystem::path path, bool isRo = true) noexcept
    {
        basePath = path;
        basePathRo = isRo;
        return *this;
    }

    ContainerCfgBuilder &setBundlePath(std::filesystem::path path) noexcept
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

    ContainerCfgBuilder &setAnnotation(ANNOTATION annotation, std::string value) noexcept;

    ContainerCfgBuilder &addUIdMapping(int64_t containerID, int64_t hostID, int64_t size) noexcept;
    ContainerCfgBuilder &addGIdMapping(int64_t containerID, int64_t hostID, int64_t size) noexcept;

    ContainerCfgBuilder &bindSys() noexcept;
    ContainerCfgBuilder &bindProc() noexcept;
    ContainerCfgBuilder &bindDev() noexcept;
    ContainerCfgBuilder &
    bindDevNode(std::function<bool(const std::string &)> ifBind = nullptr) noexcept;
    ContainerCfgBuilder &bindCgroup() noexcept;
    ContainerCfgBuilder &bindRun() noexcept;
    ContainerCfgBuilder &bindTmp() noexcept;
    ContainerCfgBuilder &bindUserGroup() noexcept;
    ContainerCfgBuilder &bindMedia() noexcept;

    ContainerCfgBuilder &forwordDefaultEnv() noexcept;
    ContainerCfgBuilder &forwordEnv(std::vector<std::string> envList) noexcept;

    ContainerCfgBuilder &bindHostRoot() noexcept;
    ContainerCfgBuilder &bindHostStatics() noexcept;
    ContainerCfgBuilder &bindHome(std::filesystem::path hostHome, std::string user) noexcept;

    ContainerCfgBuilder &enablePrivateDir() noexcept;
    ContainerCfgBuilder &mapPrivate(std::string containerPath, bool isDir) noexcept;
    ContainerCfgBuilder &bindIPC() noexcept;
    ContainerCfgBuilder &enableLDCache() noexcept;

    // TODO
    ContainerCfgBuilder &enableFontCache() noexcept { return *this; }

    ContainerCfgBuilder &enableQuirkVolatile() noexcept;

    ContainerCfgBuilder &setExtraMounts(std::vector<ocppi::runtime::config::types::Mount>) noexcept;

    ContainerCfgBuilder &
      setStartContainerHooks(std::vector<ocppi::runtime::config::types::Hook>) noexcept;

    ContainerCfgBuilder &enableSelfAdjustingMount() noexcept
    {
        selfAdjustingMountEnabled = true;
        return *this;
    }

    ContainerCfgBuilder &addMask(const std::vector<std::string> &masks) noexcept;

    ContainerCfgBuilder &isolateNetWork() noexcept
    {
        isolateNetWorkEnabled = true;
        return *this;
    }

    bool build() noexcept;

    const ocppi::runtime::config::types::Config &getConfig() const { return config; }

    Error getError() { return error_; }

    // TODO
    // ContainerCfgBuilder& mountPermission() noexcept;

    // utils::error::Result<void> useBasicConfig() noexcept;
    // utils::error::Result<void> useHostRootFSConfig() noexcept;
    // utils::error::Result<void> useHostStaticsConfig() noexcept;

    // utils::error::Result<void> addEnv(std::map<std::string, std::string> env) noexcept;

private:
    bool checkValid() noexcept;
    bool prepare() noexcept;
    bool buildIdMappings() noexcept;
    bool buildMountRuntime() noexcept;
    bool buildMountApp() noexcept;
    bool buildMountHome() noexcept;
    bool buildTmp() noexcept;
    bool buildPrivateDir() noexcept;
    bool buildPrivateMapped() noexcept;
    bool buildMountIPC() noexcept;
    bool buildMountCache() noexcept;
    bool buildLDCache() noexcept;
    bool buildQuirkVolatile() noexcept;
    bool buildEnv() noexcept;
    bool mergeMount() noexcept;
    bool finalize() noexcept;
    bool selfAdjustingMount(std::vector<ocppi::runtime::config::types::Mount> &mounts) noexcept;
    std::vector<ocppi::runtime::config::types::Mount>
    generateMounts(const std::vector<MountNode> &mountpoints,
                   std::vector<ocppi::runtime::config::types::Mount> &mounts) noexcept;

    // path settings
    std::string appId;
    std::optional<std::filesystem::path> runtimePath;
    std::optional<std::filesystem::path> appPath;
    std::optional<std::filesystem::path> basePath;
    std::optional<std::filesystem::path> bundlePath;
    std::optional<std::filesystem::path> appCache;

    bool runtimePathRo = true;
    bool appPathRo = true;
    bool basePathRo = true;
    bool appCacheRo = true;

    // id mappings
    std::optional<std::vector<ocppi::runtime::config::types::IdMapping>> uidMappings;
    std::optional<std::vector<ocppi::runtime::config::types::IdMapping>> gidMappings;

    // mount
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
    std::optional<std::vector<ocppi::runtime::config::types::Mount>> mediaMount;
    std::optional<std::vector<ocppi::runtime::config::types::Mount>> hostRootMount;
    std::optional<std::vector<ocppi::runtime::config::types::Mount>> hostStaticsMount;
    std::optional<std::vector<ocppi::runtime::config::types::Mount>> ipcMount;

    // cache
    std::optional<std::vector<ocppi::runtime::config::types::Mount>> cacheMount;
    std::optional<std::vector<ocppi::runtime::config::types::Mount>> ldCacheMount;

    // environment
    std::optional<std::vector<std::string>> envForword;
    std::map<std::string, std::string> environment;
    std::optional<ocppi::runtime::config::types::Mount> envMount;

    // home dir
    std::optional<std::filesystem::path> homePath;
    std::string homeUser;
    std::optional<std::vector<ocppi::runtime::config::types::Mount>> homeMount;

    // private dir
    std::filesystem::path privatePath;
    std::filesystem::path privateAppDir;
    std::optional<std::vector<ocppi::runtime::config::types::Mount>> privateMount;
    std::optional<std::map<std::string, bool>> privateMappings;

    // volatile
    std::optional<std::vector<ocppi::runtime::config::types::Mount>> volatileMount;

    // extra mounts
    std::optional<std::vector<ocppi::runtime::config::types::Mount>> extraMount;

    // self-adjusting mount
    bool selfAdjustingMountEnabled = false;

    bool isolateNetWorkEnabled = false;

    std::vector<std::string> maskedPaths;
    ocppi::runtime::config::types::Config config;

    Error error_;
};

}; // namespace linglong::generator
