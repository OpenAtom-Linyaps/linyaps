/*
 * SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/oci-cfg-generators/container_cfg_builder.h"

#include "configure.h"
#include "linglong/api/types/v1/Generators.hpp"
#include "linglong/api/types/v1/OciConfigurationPatch.hpp"
#include "ocppi/runtime/config/types/Generators.hpp"
#include "sha256.h"

#include <linux/limits.h>

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <vector>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

namespace linglong::generator {

using string_list = std::vector<std::string>;

using ocppi::runtime::config::types::Config;
using ocppi::runtime::config::types::Hook;
using ocppi::runtime::config::types::Hooks;
using ocppi::runtime::config::types::IdMapping;
using ocppi::runtime::config::types::Mount;
using ocppi::runtime::config::types::NamespaceReference;
using ocppi::runtime::config::types::NamespaceType;
using ocppi::runtime::config::types::Process;
using ocppi::runtime::config::types::RootfsPropagation;

namespace {
bool bindIfExist(std::vector<Mount> &mounts,
                 std::filesystem::path source,
                 std::string destination = "",
                 bool ro = true) noexcept
{
    std::error_code ec;
    if (!std::filesystem::exists(source, ec)) {
        return false;
    }

    if (destination.empty()) {
        destination = source.string();
    }

    mounts.emplace_back(Mount{ .destination = destination,
                               .options = string_list{ "rbind", ro ? "ro" : "rw" },
                               .source = source,
                               .type = "bind" });

    return true;
}
} // namespace

ContainerCfgBuilder &ContainerCfgBuilder::setAnnotation(ANNOTATION key, std::string value) noexcept
{
    if (!config.annotations) {
        config.annotations = std::map<std::string, std::string>();
    }

    switch (key) {
    case ANNOTATION::APPID:
        config.annotations->insert_or_assign("org.deepin.linglong.appID", std::move(value));
        break;
    case ANNOTATION::BASEDIR:
        config.annotations->insert_or_assign("org.deepin.linglong.baseDir", std::move(value));
        break;
    case ANNOTATION::LAST_PID:
        config.annotations->insert_or_assign("cn.org.linyaps.runtime.ns_last_pid",
                                             std::move(value));
        break;
    default:
        break;
    }

    return *this;
}

ContainerCfgBuilder &ContainerCfgBuilder::addUIdMapping(int64_t containerID,
                                                        int64_t hostID,
                                                        int64_t size) noexcept
{
    if (!uidMappings) {
        uidMappings = std::vector<IdMapping>{
            IdMapping{ .containerID = containerID, .hostID = hostID, .size = size }
        };
    } else {
        uidMappings->emplace_back(
          IdMapping{ .containerID = containerID, .hostID = hostID, .size = size });
    }

    return *this;
}

ContainerCfgBuilder &ContainerCfgBuilder::addGIdMapping(int64_t containerID,
                                                        int64_t hostID,
                                                        int64_t size) noexcept
{
    if (!gidMappings) {
        gidMappings = std::vector<IdMapping>{
            IdMapping{ .containerID = containerID, .hostID = hostID, .size = size }
        };
    } else {
        gidMappings->emplace_back(
          IdMapping{ .containerID = containerID, .hostID = hostID, .size = size });
    }

    return *this;
}

ContainerCfgBuilder &ContainerCfgBuilder::bindDefault() noexcept
{
    bindSys();
    bindProc();
    bindDev();
    bindTmp();

    return *this;
}

ContainerCfgBuilder &ContainerCfgBuilder::bindSys() noexcept
{
    sysMount = Mount{ .destination = "/sys",
                      .options = string_list{ "rbind", "nosuid", "noexec", "nodev" },
                      .source = "/sys",
                      .type = "bind" };

    return *this;
}

ContainerCfgBuilder &ContainerCfgBuilder::bindProc() noexcept
{
    procMount = Mount{ .destination = "/proc",
                       .options = string_list{ "nosuid", "noexec", "nodev" },
                       .source = "/proc",
                       .type = "proc" };

    return *this;
}

ContainerCfgBuilder &ContainerCfgBuilder::bindDev() noexcept
{
    devMount = {
        Mount{ .destination = "/dev",
               .options = string_list{ "nosuid", "strictatime", "mode=0755", "size=65536k" },
               .source = "tmpfs",
               .type = "tmpfs" },
        Mount{ .destination = "/dev/pts",
               .options =
                 string_list{ "nosuid", "noexec", "newinstance", "ptmxmode=0666", "mode=0620" },
               .source = "devpts",
               .type = "devpts" },
        Mount{ .destination = "/dev/shm",
               .options = string_list{ "nosuid", "noexec", "nodev", "mode=1777" },
               .source = "shm",
               .type = "tmpfs" },
        Mount{ .destination = "/dev/mqueue",
               .options = string_list{ "rbind", "nosuid", "noexec", "nodev" },
               .source = "/dev/mqueue",
               .type = "bind" },
    };

    return *this;
}

ContainerCfgBuilder &
ContainerCfgBuilder::bindDevNode(std::function<bool(const std::string &)> ifBind) noexcept
{
    if (!ifBind) {
        ifBind = [](const std::string &name) -> bool {
            if (name == "snd" || name == "dri" || name.rfind("video", 0) == 0
                || name.rfind("nvidia", 0) == 0) {
                return true;
            }
            return false;
        };
    }

    for (const auto &entry : std::filesystem::directory_iterator{ "/dev" }) {
        if (ifBind(entry.path().filename())) {
            Mount m{ .destination = entry.path(),
                     .options = string_list{ "rbind" },
                     .source = entry.path(),
                     .type = "bind" };

            if (devNodeMount) {
                devNodeMount->emplace_back(std::move(m));
            } else {
                devNodeMount = { std::move(m) };
            }
        }
    }

    return *this;
}

ContainerCfgBuilder &ContainerCfgBuilder::bindCgroup() noexcept
{
    cgroupMount = Mount{ .destination = "/sys/fs/cgroup",
                         .options = string_list{ "nosuid", "noexec", "nodev", "relatime", "ro" },
                         .source = "cgroup",
                         .type = "cgroup" };

    return *this;
}

ContainerCfgBuilder &ContainerCfgBuilder::bindRun() noexcept
{
    std::string containerXDGRuntimeDir = std::string("/run/user/") + std::to_string(::getuid());

    runMount = {
        Mount{ .destination = "/run",
               .options = string_list{ "nosuid", "strictatime", "mode=0755", "size=65536k" },
               .source = "tmpfs",
               .type = "tmpfs" },
        Mount{ .destination = "/run/user",
               .options = string_list{ "nodev", "nosuid", "mode=700" },
               .source = "tmpfs",
               .type = "tmpfs" },
        Mount{ .destination = containerXDGRuntimeDir,
               .options = string_list{ "nodev", "nosuid", "mode=700" },
               .source = "tmpfs",
               .type = "tmpfs" },
    };

    environment["XDG_RUNTIME_DIR"] = containerXDGRuntimeDir;

    return *this;
}

ContainerCfgBuilder &ContainerCfgBuilder::bindTmp() noexcept
{
    tmpMount = Mount{
        .destination = "/tmp",
        .options = string_list{ "rbind" },
        .source = "/tmp",
        .type = "bind",
    };

    return *this;
}

ContainerCfgBuilder &ContainerCfgBuilder::bindUserGroup() noexcept
{
    UGMount = {
        Mount{ .destination = "/etc/passwd",
               .options = string_list{ "rbind", "ro" },
               .source = "/etc/passwd",
               .type = "bind" },
        Mount{ .destination = "/etc/group",
               .options = string_list{ "rbind", "ro" },
               .source = "/etc/group",
               .type = "bind" },
    };

    return *this;
}

ContainerCfgBuilder &ContainerCfgBuilder::bindRemovableStorageMounts() noexcept
{
    // 绑定可移动存储设备的挂载点
    // /media: 自动挂载可移动设备（U盘、光盘等）
    // /mnt: 系统管理员临时挂载文件系统的标准挂载点
    std::error_code ec;

    std::vector<std::string> propagationPaths{ "/media", "/mnt" };

    for (const auto &path : propagationPaths) {
        do {
            auto mountDir = std::filesystem::path(path);
            auto status = std::filesystem::symlink_status(mountDir, ec);
            if (ec) {
                break;
            }

            if (status.type() == std::filesystem::file_type::symlink) {
                auto targetDir = std::filesystem::read_symlink(mountDir, ec);
                if (ec) {
                    break;
                }

                auto destinationDir = "/" + targetDir.string();
                if (!std::filesystem::exists(destinationDir, ec)) {
                    break;
                }

                removableStorageMounts = {
                    Mount{ .destination = destinationDir,
                           .options = string_list{ "rbind" },
                           .source = destinationDir,
                           .type = "bind" },
                    Mount{ .destination = path,
                           .options = string_list{ "rbind", "ro", "copy-symlink" },
                           .source = path,
                           .type = "bind" },
                };
            } else {
                removableStorageMounts = {
                    Mount{ .destination = path,
                           .options = string_list{ "rbind" },
                           .source = path,
                           .type = "bind" },
                };
            }
        } while (false);
    }

    return *this;
}

ContainerCfgBuilder &ContainerCfgBuilder::forwardDefaultEnv() noexcept
{
    return forwardEnv(std::vector<std::string>{
      "DISPLAY",
      "LANG",
      "LANGUAGE",
      "XDG_SESSION_DESKTOP",
      "D_DISABLE_RT_SCREEN_SCALE",
      "XMODIFIERS",
      "XCURSOR_SIZE", // 鼠标尺寸
      "DESKTOP_SESSION",
      "DEEPIN_WINE_SCALE",
      "XDG_CURRENT_DESKTOP",
      "XIM",
      "XDG_SESSION_TYPE",
      "XDG_RUNTIME_DIR",
      "CLUTTER_IM_MODULE",
      "QT4_IM_MODULE",
      "GTK_IM_MODULE",
      "all_proxy",
      "auto_proxy",      // 网络系统代理自动代理
      "http_proxy",      // 网络系统代理手动http代理
      "https_proxy",     // 网络系统代理手动https代理
      "ftp_proxy",       // 网络系统代理手动ftp代理
      "SOCKS_SERVER",    // 网络系统代理手动socks代理
      "no_proxy",        // 网络系统代理手动配置代理
      "USER",            // wine应用会读取此环境变量
      "QT_IM_MODULE",    // 输入法
      "LINGLONG_ROOT",   // 玲珑安装位置
      "WAYLAND_DISPLAY", // 导入wayland相关环境变量
      "QT_QPA_PLATFORM",
      "QT_WAYLAND_SHELL_INTEGRATION",
      "GDMSESSION",
      "QT_WAYLAND_FORCE_DPI",
      "GIO_LAUNCHED_DESKTOP_FILE", // 系统监视器
      "GNOME_DESKTOP_SESSION_ID", // gnome 桌面标识，有些应用会读取此变量以使用gsettings配置,
      // 如chrome
      "TERM",
      // 控制应用将渲染任务路由到 NVIDIA 独立显卡
      "__NV_PRIME_RENDER_OFFLOAD",
      // 控制应用使用哪个OpenGL厂商提供的驱动库来与显卡通信和渲染。
      "__GLX_VENDOR_LIBRARY_NAME",
      // 控制NVIDIA独立显卡在Vulkan应用程序枚举GPU时拥有更高的优先级
      "__VK_LAYER_NV_optimus",
      // 控制应用尝试使用非默认（通常是独立显卡）的GPU来执行OpenGL渲染任务
      "DRI_PRIME" });
}

ContainerCfgBuilder &
ContainerCfgBuilder::forwardEnv(const std::vector<std::string> &envList) noexcept
{
    if (!envList.empty()) {
        for (const auto &env : envList) {
            envForward.emplace(env);
        }

        return *this;
    }

    for (char **env = environ; *env != nullptr; ++env) {
        auto str = std::string_view(*env);
        auto idx = str.find('=');
        if (idx != std::string_view::npos) {
            envForward.emplace(str.begin(), str.begin() + idx);
        }
    }

    return *this;
}

ContainerCfgBuilder &
ContainerCfgBuilder::appendEnv(const std::map<std::string, std::string> &envMap) noexcept
{
    for (const auto &[key, value] : envMap) {
        if (envAppend.find(key) != envAppend.end()) {
            std::cerr << "env " << key << " is already exist" << std::endl;
        } else {
            envAppend[key] = value;
        }
    }

    return *this;
}

ContainerCfgBuilder &ContainerCfgBuilder::appendEnv(const std::string &env,
                                                    const std::string &value,
                                                    bool overwrite) noexcept
{
    if (overwrite || envAppend.find(env) == envAppend.end()) {
        envAppend[env] = value;
    } else {
        std::cerr << "env " << env << " is already exist" << std::endl;
    }

    return *this;
}

ContainerCfgBuilder &ContainerCfgBuilder::bindHostRoot() noexcept
{
    hostRootMount = {
        Mount{
          .destination = "/run/host",
          .options = string_list{ "nodev", "nosuid", "mode=700" },
          .source = "tmpfs",
          .type = "tmpfs",
        },
        Mount{
          .destination = "/run/host/rootfs",
          .options = string_list{ "rbind" },
          .source = "/",
          .type = "bind",
        },
    };

    return *this;
}

ContainerCfgBuilder &ContainerCfgBuilder::bindHostStatics() noexcept
{
    std::vector<std::filesystem::path> statics{
        "/etc/machine-id",
        // FIXME: support for host /etc/ssl, ref https://github.com/p11-glue/p11-kit
        "/usr/lib/locale",
        "/usr/share/fonts",
        "/usr/share/icons",
        "/usr/share/themes",
        "/var/cache/fontconfig",
    };

    hostStaticsMount = std::vector<Mount>{};
    for (const auto &loc : statics) {
        bindIfExist(*hostStaticsMount, loc);
    }

    return *this;
}

ContainerCfgBuilder &ContainerCfgBuilder::bindHome(std::filesystem::path hostHome) noexcept
{
    homePath = hostHome;
    homeMount = std::vector<Mount>{};

    return *this;
}

ContainerCfgBuilder &ContainerCfgBuilder::enablePrivateDir() noexcept
{
    privateMount = std::vector<Mount>{};

    return *this;
}

ContainerCfgBuilder &ContainerCfgBuilder::mapPrivate(std::string containerPath, bool isDir) noexcept
{
    if (privateMappings) {
        privateMappings->emplace(containerPath, isDir);
    } else {
        privateMappings = std::map<std::string, bool>{ { containerPath, isDir } };
    }

    return *this;
}

ContainerCfgBuilder &ContainerCfgBuilder::bindIPC() noexcept
{
    ipcMount = std::vector<Mount>{};
    return *this;
}

ContainerCfgBuilder &ContainerCfgBuilder::enableLDCache() noexcept
{
    ldCacheMount = std::vector<Mount>{};
    return *this;
}

ContainerCfgBuilder &ContainerCfgBuilder::enableQuirkVolatile() noexcept
{
    volatileMount = std::vector<Mount>{};
    return *this;
}

ContainerCfgBuilder &ContainerCfgBuilder::setExtensionMounts(std::vector<Mount> extensions) noexcept
{
    extensionMount = std::move(extensions);
    return *this;
}

ContainerCfgBuilder &ContainerCfgBuilder::addExtraMount(Mount extra) noexcept
{
    if (!extraMount) {
        extraMount = std::vector<Mount>{};
    }
    extraMount->emplace_back(std::move(extra));
    return *this;
}

ContainerCfgBuilder &ContainerCfgBuilder::addExtraMounts(std::vector<Mount> extra) noexcept
{
    if (!extraMount) {
        extraMount = std::vector<Mount>{};
    }
    std::move(extra.begin(), extra.end(), std::back_inserter(*extraMount));
    return *this;
}

ContainerCfgBuilder &ContainerCfgBuilder::setStartContainerHooks(std::vector<Hook> hooks) noexcept
{
    config.hooks = Hooks{};
    config.hooks->startContainer = hooks;

    return *this;
}

ContainerCfgBuilder &ContainerCfgBuilder::addMask(const std::vector<std::string> &masks) noexcept
{
    maskedPaths.insert(maskedPaths.end(), masks.begin(), masks.end());

    return *this;
}

std::string ContainerCfgBuilder::ldConf(const std::string &triplet) const
{
    std::vector<std::string> factors;
    std::string ldRawConf;
    auto appendLdConf = [&ldRawConf, &triplet](const std::string &prefix) {
        ldRawConf.append(prefix + "/lib\n");
        ldRawConf.append(prefix + "/lib/" + triplet + "\n");
        ldRawConf.append("include " + prefix + "/etc/ld.so.conf\n");
    };

    if (runtimePath) {
        appendLdConf(runtimeMountPoint);
        factors.push_back(runtimePath->string());
    }

    if (appPath) {
        appendLdConf(std::filesystem::path{ "/opt/apps" } / appId / "files");
        factors.push_back(appPath->string());
    }

    if (extensionMount) {
        for (const auto &extension : *extensionMount) {
            appendLdConf(extension.destination);
            if (extension.source) {
                factors.push_back(*extension.source);
            }
        }
    }

    std::sort(factors.begin(), factors.end());

    digest::SHA256 sha256;
    for (const auto &factor : factors) {
        sha256.update(reinterpret_cast<const std::byte *>(factor.c_str()), factor.size());
    }
    std::array<std::byte, 32> digest{};
    sha256.final(digest.data());

    std::stringstream stream;
    stream << "# ";
    stream << std::setfill('0') << std::hex;
    for (auto v : digest) {
        stream << std::setw(2) << static_cast<unsigned int>(v);
    }
    stream << std::endl;

    ldRawConf.insert(0, stream.str());

    return ldRawConf;
}

bool ContainerCfgBuilder::checkValid() noexcept
{
    if (appId.empty()) {
        error_.reason = "app id is empty";
        error_.code = BUILD_PARAM_ERROR;
        return false;
    }

    if (basePath.empty()) {
        error_.reason = "base path is not set";
        error_.code = BUILD_PARAM_ERROR;
        return false;
    }

    if (bundlePath.empty()) {
        error_.reason = "bundle path is empty";
        error_.code = BUILD_PARAM_ERROR;
        return false;
    }

    return true;
}

bool ContainerCfgBuilder::prepare() noexcept
{
    config.ociVersion = "1.0.1";
    config.hostname = "linglong";

    auto linux_ = ocppi::runtime::config::types::Linux{};
    linux_.rootfsPropagation = RootfsPropagation::Slave;
    linux_.namespaces = std::vector<NamespaceReference>{
        NamespaceReference{ .type = NamespaceType::Pid },
        NamespaceReference{ .type = NamespaceType::Mount },
        NamespaceReference{ .type = NamespaceType::Uts },
        NamespaceReference{ .type = NamespaceType::User },
    };
    if (isolateNetWorkEnabled) {
        linux_.namespaces->push_back(NamespaceReference{ .type = NamespaceType::Network });
    }
    config.linux_ = std::move(linux_);

    auto process = Process{ .args = string_list{ "bash" }, .cwd = "/" };
    config.process = std::move(process);

    config.root = { .path = basePath, .readonly = basePathRo };

    return true;
}

bool ContainerCfgBuilder::buildIdMappings() noexcept
{
    config.linux_->uidMappings = std::move(uidMappings);
    config.linux_->gidMappings = std::move(gidMappings);

    return true;
}

bool ContainerCfgBuilder::buildMountRuntime() noexcept
{
    if (!runtimePath) {
        return true;
    }

    std::error_code ec;
    if (!std::filesystem::exists(*runtimePath, ec)) {
        error_.reason = "runtime files is not exist";
        error_.code = BUILD_MOUNT_RUNTIME_ERROR;
        return false;
    }

    runtimeMount = Mount{ .destination = runtimeMountPoint,
                          .options = string_list{ "rbind", runtimePathRo ? "ro" : "rw" },
                          .source = *runtimePath,
                          .type = "bind" };

    return true;
}

bool ContainerCfgBuilder::buildMountApp() noexcept
{
    if (!appPath) {
        return true;
    }

    std::error_code ec;
    if (!std::filesystem::exists(*appPath, ec)) {
        error_.reason = "app files is not exist";
        error_.code = BUILD_MOUNT_APP_ERROR;
        return false;
    }

    appMount = { Mount{ .destination = "/opt",
                        .options = string_list{ "nodev", "nosuid", "mode=700" },
                        .source = "tmpfs",
                        .type = "tmpfs" },
                 Mount{ .destination = std::filesystem::path{ "/opt/apps" } / appId / "files",
                        .options = string_list{ "rbind", appPathRo ? "ro" : "rw" },
                        .source = *appPath,
                        .type = "bind" } };

    return true;
}

bool ContainerCfgBuilder::buildMountHome() noexcept
{
    if (!homePath) {
        return true;
    }

    if (homePath->empty()) {
        error_.reason = "homePath is empty";
        error_.code = BUILD_MOUNT_HOME_ERROR;
        return false;
    }

    std::error_code ec;
    if (!std::filesystem::exists(*homePath, ec)) {
        error_.reason = homePath->string() + " is not exist";
        error_.code = BUILD_MOUNT_HOME_ERROR;
        return false;
    }

    homeMount->emplace_back(Mount{ .destination = "/home",
                                   .options = string_list{ "nodev", "nosuid", "mode=700" },
                                   .source = "tmpfs",
                                   .type = "tmpfs" });

    auto containerHome = homePath->string();

    homeMount->emplace_back(Mount{ .destination = containerHome,
                                   .options = string_list{ "rbind" },
                                   .source = *homePath,
                                   .type = "bind" });
    environment["HOME"] = containerHome;

    auto mountDir = [this](const std::filesystem::path &hostDir, const std::string &containerDir) {
        std::error_code ec;
        if (!std::filesystem::exists(hostDir, ec)) {
            if (ec) {
                return false;
            }

            if (!std::filesystem::create_directories(hostDir, ec) && ec) {
                return false;
            }
        }

        homeMount->emplace_back(Mount{
          .destination = containerDir,
          .options = string_list{ "rbind" },
          .source = hostDir,
          .type = "bind",
        });

        return true;
    };

    auto *value = getenv("XDG_DATA_HOME");
    std::filesystem::path XDG_DATA_HOME =
      value ? std::filesystem::path{ value } : *homePath / ".local" / "share";
    std::string containerDataHome = containerHome + "/.local/share";
    if (XDG_DATA_HOME != containerDataHome) {
        if (!mountDir(XDG_DATA_HOME, containerDataHome)) {
            error_.reason = XDG_DATA_HOME.string() + " can't be mount";
            error_.code = BUILD_MOUNT_HOME_ERROR;
            return false;
        }
    }
    environment["XDG_DATA_HOME"] = containerDataHome;

    auto checkPrivatePath = [this](const std::filesystem::path &path) -> std::filesystem::path {
        if (!privateMount) {
            return std::filesystem::path{};
        }

        std::error_code ec;
        if (std::filesystem::exists(privateAppDir / path, ec)) {
            return privateAppDir / path;
        }

        return std::filesystem::path{};
    };

    value = getenv("XDG_CONFIG_HOME");
    std::filesystem::path XDG_CONFIG_HOME =
      value ? std::filesystem::path{ value } : *homePath / ".config";
    std::filesystem::path XDGConfigHome = XDG_CONFIG_HOME;
    auto privateConfigPath = checkPrivatePath("config");
    if (!privateConfigPath.empty()) {
        XDGConfigHome = privateConfigPath;
    }
    std::string containerConfigHome = containerHome + "/.config";
    if (XDGConfigHome != containerConfigHome) {
        if (!mountDir(XDGConfigHome, containerConfigHome)) {
            error_.reason = XDGConfigHome.string() + " can't be mount";
            error_.code = BUILD_MOUNT_HOME_ERROR;
            return false;
        }
    }
    environment["XDG_CONFIG_HOME"] = containerConfigHome;

    value = getenv("XDG_CACHE_HOME");
    std::filesystem::path XDG_CACHE_HOME =
      value ? std::filesystem::path{ value } : *homePath / ".cache";
    std::filesystem::path XDGCacheHome = XDG_CACHE_HOME;
    auto privateCachePath = checkPrivatePath("cache");
    if (!privateCachePath.empty()) {
        XDGCacheHome = privateCachePath;
    }
    std::string containerCacheHome = containerHome + "/.cache";
    if (XDGCacheHome != containerCacheHome) {
        if (!mountDir(XDGCacheHome, containerCacheHome)) {
            error_.reason = XDGCacheHome.string() + " can't be mount";
            error_.code = BUILD_MOUNT_HOME_ERROR;
            return false;
        }
    }
    environment["XDG_CACHE_HOME"] = containerCacheHome;

    value = getenv("XDG_STATE_HOME");
    std::filesystem::path XDG_STATE_HOME =
      value ? std::filesystem::path{ value } : *homePath / ".local" / "state";
    auto privateStatePath = checkPrivatePath("state");
    if (!privateStatePath.empty()) {
        XDG_STATE_HOME = privateStatePath;
    }
    std::string containerStateHome = containerHome + "/.local/state";
    if (XDG_STATE_HOME != containerStateHome) {
        if (!mountDir(XDG_STATE_HOME, containerStateHome)) {
            error_.reason = XDG_STATE_HOME.string() + " can't be mount";
            error_.code = BUILD_MOUNT_HOME_ERROR;
            return false;
        }
    }
    environment["XDG_STATE_HOME"] = containerStateHome;

    // systemd user path
    auto hostSystemdUserDir = XDG_CONFIG_HOME / "systemd" / "user";
    if ((XDG_CONFIG_HOME != containerConfigHome)
        && std::filesystem::exists(hostSystemdUserDir, ec)) {
        homeMount->emplace_back(Mount{ .destination = containerConfigHome + "/systemd/user",
                                       .options = string_list{ "rbind" },
                                       .source = hostSystemdUserDir,
                                       .type = "bind" });
    }

    // FIXME: Many applications get configurations from dconf, so we expose dconf to all
    // applications for now. If there is a better solution to fix this issue, please change the
    // following codes
    auto hostUserDconfPath = XDG_CONFIG_HOME / "dconf";
    if ((XDG_CONFIG_HOME != containerConfigHome)
        && std::filesystem::exists(hostUserDconfPath, ec)) {
        homeMount->emplace_back(Mount{ .destination = containerConfigHome + "/dconf",
                                       .options = string_list{ "rbind" },
                                       .source = hostUserDconfPath,
                                       .type = "bind" });
    }

    // for dde application theme
    auto hostDDEApiPath = XDG_CACHE_HOME / "deepin" / "dde-api";
    if ((XDG_CACHE_HOME != containerCacheHome) && std::filesystem::exists(hostDDEApiPath, ec)) {
        homeMount->emplace_back(Mount{ .destination = containerCacheHome + "/deepin/dde-api",
                                       .options = string_list{ "rbind" },
                                       .source = hostDDEApiPath,
                                       .type = "bind" });
    }

    // for xdg-user-dirs
    auto XDGUserDirs = XDG_CONFIG_HOME / "user-dirs.dirs";
    if ((XDG_CONFIG_HOME != containerConfigHome) && std::filesystem::exists(XDGUserDirs, ec)) {
        homeMount->push_back(Mount{ .destination = containerConfigHome + "/user-dirs.dirs",
                                    .options = string_list{ "rbind" },
                                    .source = XDGUserDirs,
                                    .type = "bind" });
    }

    auto XDGUserLocale = XDG_CONFIG_HOME / "user-dirs.locale";
    if ((XDG_CONFIG_HOME != containerConfigHome) && std::filesystem::exists(XDGUserLocale, ec)) {
        homeMount->push_back(Mount{ .destination = containerConfigHome + "/user-dirs.locale",
                                    .options = string_list{ "rbind" },
                                    .source = XDGUserLocale,
                                    .type = "bind" });
    }

    // NOTE:
    // Running ~/.bashrc from user home is meaningless in linglong container,
    // and might cause some issues, so we mask it with the default one.
    // https://github.com/linuxdeepin/linglong/issues/459
    constexpr auto defaultBashrc = "/etc/skel/.bashrc";
    if (std::filesystem::exists(defaultBashrc, ec)) {
        homeMount->push_back(Mount{
          .destination = *homePath / ".bashrc",
          .options = string_list{ "ro", "rbind" },
          .source = defaultBashrc,
          .type = "bind",
        });
    }

    return true;
}

bool ContainerCfgBuilder::buildPrivateDir() noexcept
{
    if (!privateMount) {
        return true;
    }

    if (!homePath) {
        error_.reason = "must bind home first";
        error_.code = BUILD_PRIVATEDIR_ERROR;
        return false;
    }

    privatePath = *homePath / ".linglong";
    privateAppDir = privatePath / appId;
    std::error_code ec;
    if (!std::filesystem::create_directories(privateAppDir, ec) && ec) {
        error_.reason = privateAppDir.string() + "can't be created";
        error_.code = BUILD_PRIVATEDIR_ERROR;
        return false;
    }

    // hide private directory
    maskedPaths.emplace_back(privatePath);

    return true;
}

bool ContainerCfgBuilder::buildPrivateMapped() noexcept
{
    if (!privateMappings) {
        return true;
    }

    if (!privateMount) {
        error_.reason = "must enable private dir first";
        error_.code = BUILD_PRIVATEMAP_ERROR;
        return false;
    }

    for (const auto &[path, isDir] : *privateMappings) {
        std::filesystem::path containerPath{ path };
        if (!containerPath.is_absolute()) {
            error_.reason = "must pass absolute path in container";
            error_.code = BUILD_PRIVATEMAP_ERROR;
            return false;
        }

        std::filesystem::path hostPath =
          privateAppDir / "private" / containerPath.lexically_relative("/");

        std::error_code ec;
        if (isDir) {
            // always create directory
            if (!std::filesystem::create_directories(hostPath, ec) && ec) {
                error_.reason = hostPath.string() + "can't be created";
                error_.code = BUILD_PRIVATEMAP_ERROR;
                return false;
            }
        }
        if (!std::filesystem::exists(hostPath, ec)) {
            error_.reason =
              std::string("mapped private path ") + hostPath.string() + " is not exist";
            error_.code = BUILD_PRIVATEMAP_ERROR;
            return false;
        }

        privateMount->emplace_back(Mount{ .destination = containerPath,
                                          .options = string_list{ "rbind" },
                                          .source = hostPath,
                                          .type = "bind" });
    }

    return true;
}

bool ContainerCfgBuilder::buildMountIPC() noexcept
{
    if (!ipcMount) {
        return true;
    }

    if (!runMount) {
        error_.reason = "/run is not bind";
        error_.code = BUILD_MOUNT_IPC_ERROR;
        return false;
    }

    bindIfExist(*ipcMount, "/tmp/.X11-unix", "", false);

    // TODO 应该参考规范文档实现更完善的地址解析支持
    // https://dbus.freedesktop.org/doc/dbus-specification.html#addresses
    [this]() mutable {
        // default value from
        // https://dbus.freedesktop.org/doc/dbus-specification.html#message-bus-types-system
        std::string systemBusEnv = "unix:path=/var/run/dbus/system_bus_socket";
        if (auto cStr = std::getenv("DBUS_SYSTEM_BUS_ADDRESS"); cStr != nullptr) {
            systemBusEnv = cStr;
        }
        // address 可能是 unix:path=/xxxx,giud=xxx 这种格式
        // 所以先将options部分提取出来，挂载时不需要关心
        std::string options;
        auto optionsPos = systemBusEnv.find(",");
        if (optionsPos != std::string::npos) {
            options = systemBusEnv.substr(optionsPos);
            systemBusEnv.resize(optionsPos);
        }
        auto systemBus = std::string_view{ systemBusEnv };
        auto suffix = std::string_view{ "unix:path=" };
        if (systemBus.rfind(suffix, 0) != 0U) {
            std::cerr << "Unexpected DBUS_SYSTEM_BUS_ADDRESS=" << systemBus << std::endl;
            return;
        }

        auto socketPath = std::filesystem::path(systemBus.substr(suffix.size()));
        std::error_code ec;
        if (!std::filesystem::exists(socketPath, ec)) {
            std::cerr << "D-Bus session bus socket not found at " << socketPath << std::endl;
            return;
        }

        ipcMount->emplace_back(Mount{ .destination = "/run/dbus/system_bus_socket",
                                      .options = string_list{ "rbind" },
                                      .source = std::move(socketPath),
                                      .type = "bind" });
        // 将提取的options再拼到容器中的环境变量
        environment["DBUS_SYSTEM_BUS_ADDRESS"] =
          std::string("unix:path=/run/dbus/system_bus_socket") + options;
    }();

    [this]() {
        auto *XDGRuntimeDirEnv = getenv("XDG_RUNTIME_DIR"); // NOLINT
        if (XDGRuntimeDirEnv == nullptr) {
            return;
        }

        auto hostXDGRuntimeDir = std::filesystem::path{ XDGRuntimeDirEnv };
        auto status = std::filesystem::status(hostXDGRuntimeDir);
        using perm = std::filesystem::perms;
        if (status.permissions() != perm::owner_all) {
            std::cerr << "The Unix permission of " << hostXDGRuntimeDir << "must be 0700."
                      << std::endl;
            return;
        }

        struct stat64 buf;
        if (::stat64(hostXDGRuntimeDir.string().c_str(), &buf) != 0) {
            std::cerr << "Failed to get state of " << hostXDGRuntimeDir << ": " << ::strerror(errno)
                      << std::endl;
            return;
        }

        if (buf.st_uid != ::getuid()) {
            std::cerr << hostXDGRuntimeDir << " doesn't belong to current user." << std::endl;
            return;
        }

        auto cognitiveXDGRuntimeDir = std::filesystem::path{ environment["XDG_RUNTIME_DIR"] };

        bindIfExist(*ipcMount,
                    hostXDGRuntimeDir / "pulse",
                    (cognitiveXDGRuntimeDir / "pulse").string(),
                    false);
        bindIfExist(*ipcMount,
                    hostXDGRuntimeDir / "gvfs",
                    (cognitiveXDGRuntimeDir / "gvfs").string(),
                    false);

        [this, &hostXDGRuntimeDir, &cognitiveXDGRuntimeDir]() {
            auto *waylandDisplayEnv = getenv("WAYLAND_DISPLAY"); // NOLINT
            if (waylandDisplayEnv == nullptr) {
                return;
            }

            auto socketPath = std::filesystem::path(hostXDGRuntimeDir) / waylandDisplayEnv;
            std::error_code ec;
            if (!std::filesystem::exists(socketPath, ec)) {
                return;
            }
            ipcMount->emplace_back(ocppi::runtime::config::types::Mount{
              .destination = cognitiveXDGRuntimeDir / waylandDisplayEnv,
              .options = string_list{ "rbind" },
              .source = socketPath,
              .type = "bind" });
        }();

        // TODO 应该参考规范文档实现更完善的地址解析支持
        // https://dbus.freedesktop.org/doc/dbus-specification.html#addresses
        [this, &cognitiveXDGRuntimeDir]() {
            std::string sessionBusEnv;
            if (auto cStr = std::getenv("DBUS_SESSION_BUS_ADDRESS"); cStr != nullptr) {
                sessionBusEnv = cStr;
            }
            if (sessionBusEnv.empty()) {
                std::cerr << "Couldn't get DBUS_SESSION_BUS_ADDRESS" << std::endl;
                return;
            }
            // address 可能是 unix:path=/xxxx,giud=xxx 这种格式
            // 所以先将options部分提取出来，挂载时不需要关心
            std::string options;
            auto optionsPos = sessionBusEnv.find(",");
            if (optionsPos != std::string::npos) {
                options = sessionBusEnv.substr(optionsPos);
                sessionBusEnv.resize(optionsPos);
            }
            auto sessionBus = std::string_view{ sessionBusEnv };
            auto suffix = std::string_view{ "unix:path=" };
            if (sessionBus.rfind(suffix, 0) != 0U) {
                std::cerr << "Unexpected DBUS_SESSION_BUS_ADDRESS=" << sessionBus << std::endl;
                return;
            }

            auto socketPath = std::filesystem::path(sessionBus.substr(suffix.size()));
            if (!std::filesystem::exists(socketPath)) {
                std::cerr << "D-Bus session bus socket not found at " << socketPath << std::endl;
                return;
            }

            auto cognitiveSessionBus = cognitiveXDGRuntimeDir / "bus";
            ipcMount->emplace_back(ocppi::runtime::config::types::Mount{
              .destination = cognitiveSessionBus,
              .options = string_list{ "rbind" },
              .source = socketPath,
              .type = "bind",
            });
            // 将提取的options再拼到容器中的环境变量
            environment["DBUS_SESSION_BUS_ADDRESS"] =
              "unix:path=" + cognitiveSessionBus.string() + options;
        }();

        [this, &hostXDGRuntimeDir, &cognitiveXDGRuntimeDir]() {
            auto dconfPath = std::filesystem::path(hostXDGRuntimeDir) / "dconf";
            if (!std::filesystem::exists(dconfPath)) {
                std::cerr << "dconf directory not found at " << dconfPath << "." << std::endl;
                return;
            }
            ipcMount->emplace_back(ocppi::runtime::config::types::Mount{
              .destination = cognitiveXDGRuntimeDir / "dconf",
              .options = string_list{ "rbind" },
              .source = dconfPath.string(),
              .type = "bind",
            });
        }();
    }();

    [this]() mutable {
        if (!homePath) {
            return;
        }

        auto hostXauthFile = *homePath / ".Xauthority";
        auto cognitiveXauthFile = homePath->string() + "/.Xauthority";

        auto *xauthFileEnv = ::getenv("XAUTHORITY"); // NOLINT
        std::error_code ec;
        if (xauthFileEnv != nullptr && std::filesystem::exists(xauthFileEnv, ec)) {
            hostXauthFile = std::filesystem::path{ xauthFileEnv };
        }

        if (!std::filesystem::exists(hostXauthFile, ec)) {
            if (ec) {
                std::cerr << "failed to check XAUTHORITY file " << hostXauthFile << ":"
                          << ec.message() << std::endl;
                return;
            }

            std::cerr << "XAUTHORITY file not found at " << hostXauthFile << ":" << ec.message()
                      << std::endl;
            return;
        }

        ipcMount->emplace_back(Mount{ .destination = cognitiveXauthFile,
                                      .options = string_list{ "rbind" },
                                      .source = hostXauthFile,
                                      .type = "bind" });
        environment["XAUTHORITY"] = cognitiveXauthFile;
    }();

    return true;
}

bool ContainerCfgBuilder::buildMountCache() noexcept
{
    if (!appCache) {
        return true;
    }

    std::error_code ec;
    if (!std::filesystem::exists(*appCache, ec)) {
        error_.reason = "app cache is not exist";
        error_.code = BUILD_MOUNT_CACHE_ERROR;
        return false;
    }

    cacheMount = { Mount{ .destination = "/run/linglong/cache",
                          .options = string_list{ "rbind", appCacheRo ? "ro" : "rw" },
                          .source = *appCache,
                          .type = "bind" } };

    return true;
}

bool ContainerCfgBuilder::buildLDCache() noexcept
{
    if (!ldCacheMount) {
        return true;
    }

    if (!appCache) {
        error_.reason = "app cache is not set";
        error_.code = BUILD_LDCACHE_ERROR;
        return false;
    }

    ldCacheMount->emplace_back(Mount{ .destination = "/etc/ld.so.cache",
                                      .options = string_list{ "rbind", "ro" },
                                      .source = *appCache / "ld.so.cache",
                                      .type = "bind" });

    return true;
}

bool ContainerCfgBuilder::buildMountLocalTime() noexcept
{
    // always bind host's localtime
    // assume /etc/localtime is a symlink to /usr/share/zoneinfo/XXX/NNN
    localtimeMount = std::vector<Mount>{};

    std::filesystem::path localtime{ "/etc/localtime" };
    std::error_code ec;
    if (std::filesystem::exists(localtime, ec)) {
        bool isSymLink = false;
        if (std::filesystem::is_symlink(localtime, ec)) {
            isSymLink = true;
        }
        localtimeMount->emplace_back(Mount{ .destination = localtime.string(),
                                            .options = isSymLink
                                              ? string_list{ "rbind", "copy-symlink" }
                                              : string_list{ "rbind", "ro" },
                                            .source = localtime,
                                            .type = "bind" });
    }

    bindIfExist(*localtimeMount, "/usr/share/zoneinfo");
    bindIfExist(*localtimeMount, "/etc/timezone");

    return true;
}

bool ContainerCfgBuilder::buildMountNetworkConf() noexcept
{
    networkConfMount = std::vector<Mount>{};

    std::filesystem::path resolvConf{ "/etc/resolv.conf" };
    std::error_code ec;
    if (std::filesystem::exists(resolvConf, ec)) {
        // /etc/resolv.conf is volatile on host, we create a new symlink in the bundle
        // directory pointing to the actual target, and then mount it with the
        // 'copy-symlink' option, which tells the runtime to recreate the symlink inside
        // the container.
        // NOTE: it's not working if /etc/resolv.conf is a symlink, and points to a
        // different path after container started.
        if (hostRootMount) {
            auto target = resolvConf;
            if (std::filesystem::is_symlink(resolvConf, ec)) {
                std::array<char, PATH_MAX + 1> buf{};
                auto *rpath = realpath(resolvConf.string().c_str(), buf.data());
                if (rpath == nullptr) {
                    error_.reason =
                      "Failed to read symlink " + resolvConf.string() + ": " + strerror(errno);
                    error_.code = BUILD_NETWORK_CONF_ERROR;
                    return false;
                }
                target = std::filesystem::path{ rpath };
            }

            target = std::filesystem::path{ "/run/host/rootfs" } / target.lexically_relative("/");
            auto bundleResolvConf = bundlePath / "resolv.conf";
            std::filesystem::create_symlink(target, bundleResolvConf, ec);
            if (ec) {
                error_.reason =
                  "Failed to create symlink " + bundleResolvConf.string() + ": " + ec.message();
                error_.code = BUILD_NETWORK_CONF_ERROR;
                return false;
            }
            networkConfMount->emplace_back(Mount{ .destination = resolvConf.string(),
                                                  .options = string_list{ "rbind", "copy-symlink" },
                                                  .source = bundleResolvConf,
                                                  .type = "bind" });
        } else {
            networkConfMount->emplace_back(Mount{ .destination = resolvConf.string(),
                                                  .options = string_list{ "rbind", "ro" },
                                                  .source = resolvConf,
                                                  .type = "bind" });
        }
    }

    bindIfExist(*networkConfMount, "/etc/resolvconf");
    bindIfExist(*networkConfMount, "/etc/hosts");
    return true;
}

// TODO
bool ContainerCfgBuilder::buildQuirkVolatile() noexcept
{
    if (!volatileMount) {
        return true;
    }

    if (!hostRootMount) {
        error_.reason = "/run/host/rootfs must mount first";
        error_.code = BUILD_MOUNT_VOLATILE_ERROR;
        return false;
    }

    return false;
}

bool ContainerCfgBuilder::buildEnv() noexcept
{
    for (const auto &key : envForward) {
        auto *value = getenv(key.c_str());
        if (value != nullptr) {
            environment.emplace(key, value);
        }
    }

    for (const auto &[key, value] : envAppend) {
        environment[key] = value;
    }

    if (appPath) {
        environment["LINGLONG_APPID"] = appId;
    }

    auto envShFile = bundlePath / "00env.sh";
    std::ofstream ofs(envShFile);
    if (!ofs.is_open()) {
        error_.reason = envShFile.string() + " can't be created";
        error_.code = BUILD_ENV_ERROR;
        return false;
    }

    auto env = std::vector<std::string>{};
    for (const auto &[key, value] : environment) {
        env.emplace_back(key + "=" + value);

        // here we process environment variables with single quotes.
        // A=a'b ===> A='a'\''b'
        std::string escaped;
        for (auto it = value.begin(); it != value.end(); ++it) {
            if (*it == '\'') {
                escaped.append(R"('\'')");
            } else {
                escaped.push_back(*it);
            }
        }
        ofs << "export " << key << "='" << escaped << "'" << std::endl;
    }
    ofs.close();

    config.process->env = std::move(env);

    envMount = Mount{ .destination = "/etc/profile.d/00env.sh",
                      .options = string_list{ "rbind", "ro" },
                      .source = envShFile,
                      .type = "bind" };

    return true;
}

bool ContainerCfgBuilder::applyPatch() noexcept
{
    if (!applyPatchEnabled) {
        return true;
    }

    std::filesystem::path containerConfigPath{ LINGLONG_INSTALL_PREFIX
                                               "/lib/linglong/container/config.d" };
    std::error_code ec;
    if (!std::filesystem::exists(containerConfigPath, ec)) {
        // if no-exists or failed to check exists, ignore it
        return true;
    }

    std::vector<std::filesystem::path> globalPatchFiles;
    std::vector<std::filesystem::path> appPatchFiles;
    auto iter = std::filesystem::directory_iterator{
        containerConfigPath,
        std::filesystem::directory_options::skip_permission_denied,
        ec
    };
    if (ec) {
        error_.reason =
          "failed to iterator directory " + containerConfigPath.string() + ": " + ec.message();
        error_.code = BUILD_PREPARE_ERROR;
        return false;
    }
    for (const auto &entry : iter) {
        const auto &path = entry.path();
        if (entry.is_regular_file(ec)) {
            globalPatchFiles.emplace_back(path);
        } else if (entry.is_directory(ec)) {
            if (path.filename().string() == appId) {
                auto iterApp = std::filesystem::directory_iterator{
                    path,
                    std::filesystem::directory_options::skip_permission_denied,
                    ec
                };
                if (ec) {
                    error_.reason =
                      "failed to iterator directory " + path.string() + ": " + ec.message();
                }
                for (const auto &entryApp : iterApp) {
                    if (!entryApp.is_regular_file(ec)) {
                        continue;
                    }
                    appPatchFiles.emplace_back(entryApp.path());
                }
            }
        }
    }
    std::sort(globalPatchFiles.begin(), globalPatchFiles.end());
    std::sort(appPatchFiles.begin(), appPatchFiles.end());

    auto doPatch = [this](const std::vector<std::filesystem::path> &patchFiles) -> bool {
        for (const auto &patchFile : patchFiles) {
            applyPatchFile(patchFile);
        }
        return true;
    };

    if (!doPatch(globalPatchFiles) || !doPatch(appPatchFiles)) {
        return false;
    }

    return true;
}

bool ContainerCfgBuilder::applyPatchFile(const std::filesystem::path &patchFile) noexcept
{
    std::error_code ec;
    auto status = std::filesystem::status(patchFile, ec);
    if (ec) {
        std::cerr << "Failed to get status of patch file " << patchFile << ": " << ec.message()
                  << std::endl;
        return true;
    }

    if ((status.permissions() & std::filesystem::perms::owner_exec) != std::filesystem::perms::none
        || (status.permissions() & std::filesystem::perms::group_exec)
          != std::filesystem::perms::none
        || (status.permissions() & std::filesystem::perms::others_exec)
          != std::filesystem::perms::none) {
        applyExecutablePatch(patchFile);
        return true;
    }

    if (patchFile.extension() == ".json") {
        // skip if failed to apply
        applyJsonPatchFile(patchFile);
        return true;
    }

    std::cerr << "Patch file " << patchFile
              << " is not an executable or a JSON patch file, skipping." << std::endl;
    return true;
}

bool ContainerCfgBuilder::applyJsonPatchFile(const std::filesystem::path &patchFile) noexcept
{
    std::ifstream file(patchFile);
    if (!file.is_open()) {
        std::cerr << "Failed to open file " << patchFile << std::endl;
        return false;
    }

    try {
        auto json = nlohmann::json::parse(file);
        auto patchContent = json.get<linglong::api::types::v1::OciConfigurationPatch>();

        if (config.ociVersion != patchContent.ociVersion) {
            std::cerr << "ociVersion mismatched " << patchFile << std::endl;
            return false;
        }

        auto raw = nlohmann::json(config);
        auto patchedJson = raw.patch(patchContent.patch);
        config = patchedJson.get<Config>();
    } catch (const std::exception &e) {
        std::cerr << "Failed to apply JSON patch " << patchFile << ": " << e.what() << std::endl;
        return false;
    }

    return true;
}

bool ContainerCfgBuilder::applyExecutablePatch(const std::filesystem::path &patchFile) noexcept
{
    std::string command = patchFile.string();
    std::string inputJsonStr;
    try {
        inputJsonStr = nlohmann::json(config).dump();
    } catch (const std::exception &e) {
        error_.reason = std::string("Failed to serialize config: ") + e.what();
        error_.code = BUILD_PREPARE_ERROR;
        return false;
    }

    int stdinPipe[2];
    int stdoutPipe[2];
    if (pipe(stdinPipe) == -1) {
        error_.reason = std::string("Failed to create stdin pipe: ") + strerror(errno);
        error_.code = BUILD_PREPARE_ERROR;
        return false;
    }
    if (pipe(stdoutPipe) == -1) {
        close(stdinPipe[0]);
        close(stdinPipe[1]);
        error_.reason = std::string("Failed to create stdout pipe: ") + strerror(errno);
        error_.code = BUILD_PREPARE_ERROR;
        return false;
    }

    pid_t pid = fork();
    if (pid == -1) {
        close(stdinPipe[0]);
        close(stdinPipe[1]);
        close(stdoutPipe[0]);
        close(stdoutPipe[1]);
        error_.reason = "Failed to fork " + command + ": " + strerror(errno);
        error_.code = BUILD_PREPARE_ERROR;
        return false;
    }

    if (pid == 0) { // Child process
        close(stdinPipe[1]);
        close(stdoutPipe[0]);

        if (dup2(stdinPipe[0], STDIN_FILENO) == -1) {
            perror(("dup2 stdin failed for " + command).c_str());
            _exit(EXIT_FAILURE);
        }
        if (dup2(stdoutPipe[1], STDOUT_FILENO) == -1) {
            perror(("dup2 stdout failed for " + command).c_str());
            _exit(EXIT_FAILURE);
        }

        close(stdinPipe[0]);
        close(stdoutPipe[1]);

        execl(patchFile.c_str(), patchFile.filename().c_str(), (char *)nullptr);

        // If execl returns, it's an error
        perror(("execl failed for " + command).c_str());
        _exit(127);
    }

    // Parent process
    close(stdinPipe[0]);  // Close read end of stdin pipe
    close(stdoutPipe[1]); // Close write end of stdout pipe

    size_t bytesWritten = 0;
    while (bytesWritten < inputJsonStr.size()) {
        ssize_t n = write(stdinPipe[1],
                          inputJsonStr.c_str() + bytesWritten,
                          inputJsonStr.size() - bytesWritten);
        if (n == -1) {
            if (errno == EINTR) {
                continue;
            }
            error_.reason = "Failed to write to stdin of " + command + ": " + strerror(errno);
            error_.code = BUILD_PREPARE_ERROR;
            close(stdinPipe[1]); // Attempt to close before waiting
            close(stdoutPipe[0]);
            waitpid(pid, nullptr, 0); // Clean up child
            return false;
        }
        bytesWritten += n;
    }
    close(stdinPipe[1]); // Close write end to signal EOF to child

    std::stringstream outputJson;
    char buffer[4096];
    ssize_t bytesRead;
    while ((bytesRead = read(stdoutPipe[0], buffer, sizeof(buffer))) > 0) {
        outputJson.write(buffer, bytesRead);
    }
    close(stdoutPipe[0]); // Close read end

    if (bytesRead == -1 && errno != EINTR
        && errno != 0) { // EINTR is ok, 0 means EOF was already hit
        error_.reason = "Failed to read from stdout of " + command + ": " + strerror(errno);
        error_.code = BUILD_PREPARE_ERROR;
        waitpid(pid, nullptr, 0); // Clean up child
        return false;
    }

    int status;
    waitpid(pid, &status, 0);

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        std::string exitInfo;
        if (WIFEXITED(status)) {
            exitInfo = "exited with status " + std::to_string(WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            exitInfo = "killed by signal " + std::to_string(WTERMSIG(status));
        } else {
            exitInfo = "terminated abnormally";
        }
        error_.reason = "Command " + command + " " + exitInfo + ". Output: " + outputJson.str();
        error_.code = BUILD_PREPARE_ERROR;
        return false;
    }

    std::string outputJsonStr = outputJson.str();
    try {
        config = nlohmann::json::parse(outputJsonStr).get<Config>();
    } catch (const std::exception &e) {
        error_.reason = "Failed to process output from " + command + ": " + e.what()
          + ". Output: " + outputJsonStr;
        error_.code = BUILD_PREPARE_ERROR;
        return false;
    }
    return true;
}

bool ContainerCfgBuilder::mergeMount() noexcept
{
    // merge all mounts here, the order of mounts is relevant
    if (runtimeMount) {
        mounts.insert(mounts.end(), std::move(*runtimeMount));
    }

    if (appMount) {
        std::move(appMount->begin(), appMount->end(), std::back_inserter(mounts));
    }

    if (extensionMount) {
        std::move(extensionMount->begin(), extensionMount->end(), std::back_inserter(mounts));
    }

    if (sysMount) {
        mounts.insert(mounts.end(), std::move(*sysMount));
    }

    if (procMount) {
        mounts.insert(mounts.end(), std::move(*procMount));
    }

    if (devMount) {
        std::move(devMount->begin(), devMount->end(), std::back_inserter(mounts));
    }

    if (devNodeMount) {
        std::move(devNodeMount->begin(), devNodeMount->end(), std::back_inserter(mounts));
    }

    if (cgroupMount) {
        mounts.insert(mounts.end(), std::move(*cgroupMount));
    }

    if (runMount) {
        std::move(runMount->begin(), runMount->end(), std::back_inserter(mounts));
    }

    if (tmpMount) {
        mounts.insert(mounts.end(), std::move(*tmpMount));
    }

    if (UGMount) {
        std::move(UGMount->begin(), UGMount->end(), std::back_inserter(mounts));
    }

    if (removableStorageMounts) {
        std::move(removableStorageMounts->begin(), removableStorageMounts->end(), std::back_inserter(mounts));
    }

    if (hostRootMount) {
        std::move(hostRootMount->begin(), hostRootMount->end(), std::back_inserter(mounts));
    }

    if (hostStaticsMount) {
        std::move(hostStaticsMount->begin(), hostStaticsMount->end(), std::back_inserter(mounts));
    }

    if (homeMount) {
        std::move(homeMount->begin(), homeMount->end(), std::back_inserter(mounts));
    }

    if (ipcMount) {
        std::move(ipcMount->begin(), ipcMount->end(), std::back_inserter(mounts));
    }

    if (cacheMount) {
        std::move(cacheMount->begin(), cacheMount->end(), std::back_inserter(mounts));
    }

    if (ldCacheMount) {
        std::move(ldCacheMount->begin(), ldCacheMount->end(), std::back_inserter(mounts));
    }

    if (localtimeMount) {
        std::move(localtimeMount->begin(), localtimeMount->end(), std::back_inserter(mounts));
    }

    if (networkConfMount) {
        std::move(networkConfMount->begin(), networkConfMount->end(), std::back_inserter(mounts));
    }

    if (privateMount) {
        std::move(privateMount->begin(), privateMount->end(), std::back_inserter(mounts));
    }

    if (envMount) {
        mounts.insert(mounts.end(), std::move(*envMount));
    }

    if (extraMount) {
        std::move(extraMount->begin(), extraMount->end(), std::back_inserter(mounts));
    }

    config.mounts = std::move(mounts);

    return true;
}

int ContainerCfgBuilder::findChild(int parent, const std::string &name) noexcept
{
    for (auto idx : mountpoints[parent].childs_idx) {
        if (mountpoints[idx].name == name) {
            return idx;
        }
    }

    return -1;
}

int ContainerCfgBuilder::insertChild(int parent, MountNode node) noexcept
{
    node.parent_idx = parent;
    mountpoints.emplace_back(std::move(node));
    int child = mountpoints.size() - 1;
    mountpoints[parent].childs_idx.push_back(child);
    return child;
}

int ContainerCfgBuilder::insertChildRecursively(const std::filesystem::path &path,
                                                bool &inserted) noexcept
{
    int currentNodeIndex = 0; // start from root (index 0)
    inserted = false;

    for (const auto &part : path) {
        std::string component = part.string();
        if (component.empty() || component == "/") {
            continue;
        }

        int childIndex = findChild(currentNodeIndex, component);
        if (childIndex == -1) {
            childIndex = insertChild(currentNodeIndex,
                                     MountNode{ .name = component, .ro = true, .mount_idx = -1 });
            inserted = true;
        }
        currentNodeIndex = childIndex;
    }

    return currentNodeIndex;
}

int ContainerCfgBuilder::findNearestMountNode(int node) noexcept
{
    while (node > 0) {
        node = mountpoints[node].parent_idx;

        if (mountpoints[node].mount_idx >= 0) {
            return node;
        }
    }

    return 0;
}

bool ContainerCfgBuilder::shouldFix(int node, std::filesystem::path &fixPath) noexcept
{
    // it's not a mount point
    int idx = mountpoints[node].mount_idx;
    if (idx < 0) {
        return false;
    }

    int mounted = findNearestMountNode(node);

    std::string root;
    if (mounted == 0) {
        std::filesystem::path r{ config.root->path };
        // assume bundle path is writable
        if (!r.is_absolute()) {
            return false;
        }
        root = r;
    } else {
        const auto &mount = mounts[mountpoints[mounted].mount_idx];
        if (mount.type != "bind") {
            return false;
        }

        if (!mount.source) {
            return false;
        }
        root = mount.source.value();
    }

    // only bind from layers should fix
    if (!(root.rfind(basePath, 0) == 0 || (runtimePath && root.rfind(*runtimePath, 0) == 0)
          || (appPath && root.rfind(*appPath, 0) == 0))) {
        return false;
    }

    auto hostPath = std::filesystem::path{ root } / getRelativePath(mounted, node);
    std::error_code ec;

    auto isCopySymlink = [this](int node) {
        const auto &mount = mounts[mountpoints[node].mount_idx];
        auto find = std::find_if(mount.options->begin(), mount.options->end(), [](const auto &opt) {
            return opt == "copy-symlink";
        });
        return find != mount.options->end();
    };
    // node should fix if the file
    // 1. is /etc/localtime or
    // 2. is not exist or
    // 3. is not a symlink but mount with option copy-symlink
    if (getRelativePath(0, node) == "etc/localtime" || !std::filesystem::exists(hostPath, ec)
        || ((!std::filesystem::is_symlink(hostPath, ec)) && isCopySymlink(node))) {
        fixPath = std::move(hostPath);
        return true;
    }

    return false;
}

std::string ContainerCfgBuilder::getRelativePath(int parent, int node) noexcept
{
    std::filesystem::path path;
    while (node != parent) {
        if (node <= 0) {
            break;
        }

        const auto &mp = mountpoints[node];
        if (path.empty())
            path = mp.name;
        else
            path = mp.name / path;
        node = mp.parent_idx;
    }

    return path.string();
}

bool ContainerCfgBuilder::adjustNode(int node,
                                     const std::filesystem::path &path,
                                     const std::filesystem::path fixPath) noexcept
{
    // adjust "node" to tmpfs and bind all child entry under "path" except "fixPath"
    auto destination = getRelativePath(0, node);
    if (!destination.empty() && destination.front() != '/') {
        destination = "/" + destination;
    }

    bool isRo = false;
    if (node != 0) {
        auto &mp = mountpoints[node];
        if (mp.mount_idx >= 0) {
            auto &fixMount = mounts[mp.mount_idx];
            fixMount.options = string_list{ "nodev", "nosuid", "mode=700" };
            fixMount.source = "tmpfs";
            fixMount.type = "tmpfs";
        } else {
            mounts.emplace_back(Mount{ .destination = destination,
                                       .options = string_list{ "nodev", "nosuid", "mode=700" },
                                       .source = "tmpfs",
                                       .type = "tmpfs" });
            mp.mount_idx = mounts.size() - 1;
        }
    }
    isRo = mountpoints[node].ro;

    for (auto const &entry : std::filesystem::directory_iterator{ path }) {
        // skip the path that trigger adjustNode
        if (entry.path() == fixPath) {
            continue;
        }

        auto filename = entry.path().filename();
        // if not bind, bind it and mark with fix flag
        auto child = findChild(node, filename);
        if (child > 0 && mountpoints[child].mount_idx >= 0) {
            continue;
        }

        auto mount = Mount{ .destination = destination + "/" + filename.string(),
                            .options = string_list{ "rbind", isRo ? "ro" : "rw" },
                            .source = path / filename,
                            .type = "bind" };
        if (entry.is_symlink()) {
            mount.options->emplace_back("copy-symlink");
        }
        mounts.emplace_back(std::move(mount));

        if (child < 0) {
            insertChild(
              node,
              MountNode{ .name = filename, .mount_idx = static_cast<int>(mounts.size() - 1) });
        } else {
            mountpoints[child].mount_idx = static_cast<int>(mounts.size() - 1);
        }
    }

    return true;
}

bool ContainerCfgBuilder::constructMountpointsTree() noexcept
{
    // root always at 0
    mountpoints.emplace_back(
      MountNode{ .name = "",
                 .ro = config.root->readonly ? config.root->readonly.value() : false,
                 .mount_idx = -1,
                 .parent_idx = -1 });

    // construct prefix tree
    for (size_t i = 0; i < mounts.size(); ++i) {
        const auto &mount = mounts[i];

        std::filesystem::path destination = std::filesystem::path{ mount.destination };
        if (destination.empty() || !destination.is_absolute()) {
            error_.reason = destination.string() + " as mount destination is invalid";
            error_.code = BUILD_MOUNT_ERROR;
            return false;
        }

        bool inserted = false;
        int child = insertChildRecursively(destination, inserted);
        auto &mp = mountpoints[child];

        if (inserted) {
            // attach to mounts
            mp.mount_idx = static_cast<int>(i);
            if (mount.options) {
                auto find =
                  std::find_if(mount.options->begin(), mount.options->end(), [](const auto &opt) {
                      return opt == "ro";
                  });
                mp.ro = find != mount.options->end();
            }
        }
    }

    return true;
}

void ContainerCfgBuilder::tryFixMountpointsTree() noexcept
{
    // Perform a pre-order tree traversal to collect the nodes to be processed
    std::vector<int> nodesToProcess;
    size_t idx = 0;
    int node = 0;
    do {
        for (auto i : mountpoints[node].childs_idx) {
            nodesToProcess.push_back(i);
        }

        if (idx >= nodesToProcess.size())
            break;
        node = nodesToProcess[idx++];
    } while (true);

    // Traverse the nodes to be processed in reverse order to ensure child nodes are handled before
    // their parent nodes.
    for (auto it = nodesToProcess.rbegin(); it != nodesToProcess.rend(); ++it) {
        std::filesystem::path fixPath;
        if (shouldFix(*it, fixPath)) {
            int node = *it;
            auto path = fixPath;
            // find the nearest ancestor node exist on host
            while (path.has_relative_path()) {
                path = path.parent_path();
                node = mountpoints[node].parent_idx;
                std::error_code ec;
                if (std::filesystem::exists(path, ec)) {
                    adjustNode(node, path, fixPath);
                    break;
                }
            }
        }
    }
}

void ContainerCfgBuilder::generateMounts() noexcept
{
    // use BFS to travel mountpoints tree to generate the mounts
    std::vector<Mount> generated;
    std::vector<int> queue = { 0 };
    size_t idx = 0;
    int node = 0;

    while (queue.size() > idx) {
        node = queue[idx++];
        for (auto i : mountpoints[node].childs_idx) {
            queue.push_back(i);
            const auto &child = mountpoints[i];
            if (child.mount_idx >= 0) {
                generated.emplace_back(mounts[child.mount_idx]);
            }
        }
    }

    mounts = std::move(generated);
}

bool ContainerCfgBuilder::selfAdjustingMount() noexcept
{
    if (!selfAdjustingMountEnabled) {
        return true;
    }

    mounts = std::move(config.mounts).value();

    // Some apps depends on files which doesn't exist in runtime layer or base layer, we have to
    // mount host files to container, or create the file on demand, but the layer is readonly. We
    // make a workaround by mount the suitable target's ancestor directory as tmpfs.
    if (!constructMountpointsTree()) {
        return false;
    }

    // Remounting as tmpfs requires an alternate rootfs context to avoid obscuring underlying files,
    // so adjust root and change root path to bundlePath/rootfs
    adjustNode(0, config.root->path, "");
    auto rootfs = bundlePath / "rootfs";
    std::error_code ec;
    if (!std::filesystem::create_directories(rootfs, ec) && ec) {
        error_.reason = rootfs.string() + " can't be created";
        error_.code = BUILD_PREPARE_ERROR;
        return false;
    }
    config.root->path = "rootfs";

    tryFixMountpointsTree();

    generateMounts();

    config.mounts = std::move(mounts);

    return true;
}

bool ContainerCfgBuilder::finalize() noexcept
{
    config.linux_->maskedPaths = maskedPaths;
    return true;
}

bool ContainerCfgBuilder::build() noexcept
{
    if (!checkValid()) {
        return false;
    }

    if (!prepare()) {
        return false;
    }

    if (!buildIdMappings()) {
        return false;
    }

    if (!buildMountRuntime() || !buildMountApp()) {
        return false;
    }

    if (!buildPrivateDir() || !buildMountHome() || !buildPrivateMapped()) {
        return false;
    }

    if (!buildMountIPC() || !buildMountLocalTime() || !buildMountNetworkConf()) {
        return false;
    }

    if (!buildMountCache() || !buildLDCache()) {
        return false;
    }

    if (!buildEnv()) {
        return false;
    }

    if (!mergeMount()) {
        return false;
    }

    if (!finalize()) {
        return false;
    }

    if (!applyPatch()) {
        return false;
    }

    if (!selfAdjustingMount()) {
        return false;
    }

    return true;
}

} // namespace linglong::generator
