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
#include <unordered_map>
#include <unordered_set>

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

    std::optional<std::filesystem::path> getRuntimePath() { return runtimePath; }

    ContainerCfgBuilder &setBasePath(std::filesystem::path path, bool isRo = true) noexcept
    {
        basePath = path;
        basePathRo = isRo;
        return *this;
    }

    std::optional<std::filesystem::path> getBasePath() { return basePath; }

    ContainerCfgBuilder &setBundlePath(const std::filesystem::path &path) noexcept
    {
        bundlePath = path;
        return *this;
    }

    const std::filesystem::path &getBundlePath() const noexcept { return bundlePath; }

    ContainerCfgBuilder &setAppCache(std::filesystem::path path, bool isRo = true) noexcept
    {
        appCache = path;
        appCacheRo = isRo;
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
    ContainerCfgBuilder &bindRun() noexcept;
    ContainerCfgBuilder &bindTmp() noexcept;
    ContainerCfgBuilder &bindUserGroup() noexcept;
    ContainerCfgBuilder &bindMedia() noexcept;

    ContainerCfgBuilder &forwardDefaultEnv() noexcept;
    ContainerCfgBuilder &forwardEnv(const std::vector<std::string> &envList = {}) noexcept;
    ContainerCfgBuilder &appendEnv(const std::map<std::string, std::string> &envMap) noexcept;

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

    ContainerCfgBuilder &
      setExtensionMounts(std::vector<ocppi::runtime::config::types::Mount>) noexcept;
    ContainerCfgBuilder &addExtraMount(ocppi::runtime::config::types::Mount) noexcept;
    ContainerCfgBuilder &addExtraMounts(std::vector<ocppi::runtime::config::types::Mount>) noexcept;

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

    ContainerCfgBuilder &disablePatch() noexcept
    {
        applyPatchEnabled = false;
        return *this;
    }

    std::string ldConf(const std::string &triplet);

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
    bool applyPatch() noexcept;
    bool applyPatchFile(const std::filesystem::path &patchFile) noexcept;
    bool applyJsonPatchFile(const std::filesystem::path &patchFile) noexcept;
    bool applyExecutablePatch(const std::filesystem::path &patchFile) noexcept;
    bool mergeMount() noexcept;
    bool finalize() noexcept;

    // adjust mount
    int findChild(int parent, const std::string &name) noexcept;
    int insertChild(int parent, MountNode node) noexcept;
    int insertChildRecursively(const std::filesystem::path &path, bool &inserted) noexcept;
    int findNearestMountNode(int child) noexcept;
    bool shouldFix(int node, std::filesystem::path &fixPath) noexcept;
    std::string getRelativePath(int parent, int node) noexcept;
    bool adjustNode(int node,
                    const std::filesystem::path &path,
                    const std::filesystem::path fixPath) noexcept;
    bool constructMountpointsTree() noexcept;
    void tryFixMountpointsTree() noexcept;
    void generateMounts() noexcept;
    bool selfAdjustingMount() noexcept;

    // path settings
    std::string appId;
    std::optional<std::filesystem::path> runtimePath;
    std::optional<std::filesystem::path> appPath;
    std::filesystem::path basePath;
    std::filesystem::path bundlePath;
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
    std::unordered_set<std::string> envForward;
    std::unordered_map<std::string, std::string> environment;
    std::unordered_map<std::string, std::string> envAppend;
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

    // extension mounts
    std::optional<std::vector<ocppi::runtime::config::types::Mount>> extensionMount;
    // extra mounts
    std::optional<std::vector<ocppi::runtime::config::types::Mount>> extraMount;

    // self-adjusting mount
    bool selfAdjustingMountEnabled = false;
    // mountpoints is a prefix tree of all mounts path
    // .mount_idx > 0 represents the path is a mount point, and it's the subscript of the array
    // mounts
    std::vector<MountNode> mountpoints;
    // this 'mounts' is used internally, distinct from config.mounts
    std::vector<ocppi::runtime::config::types::Mount> mounts;

    bool isolateNetWorkEnabled = false;
    bool applyPatchEnabled = true;

    std::vector<std::string> maskedPaths;
    ocppi::runtime::config::types::Config config;

    Error error_;

    const std::string runtimeMountPoint = "/runtime";
};

}; // namespace linglong::generator
