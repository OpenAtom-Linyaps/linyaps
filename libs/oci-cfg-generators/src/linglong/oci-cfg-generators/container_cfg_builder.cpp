/*
 * SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/oci-cfg-generators/container_cfg_builder.h"

#include "ocppi/runtime/config/types/Generators.hpp"

#include <fstream>
#include <iostream>

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

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
        Mount{ .destination = "/run/udev",
               .options = string_list{ "rbind" },
               .source = "/run/udev",
               .type = "bind" },
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
    tmpMount = Mount{};

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

ContainerCfgBuilder &ContainerCfgBuilder::bindMedia() noexcept
{
    std::error_code ec;
    do {
        auto mediaDir = std::filesystem::path("/media");
        auto status = std::filesystem::symlink_status(mediaDir, ec);
        if (ec) {
            break;
        }

        if (status.type() == std::filesystem::file_type::symlink) {
            auto targetDir = std::filesystem::read_symlink(mediaDir, ec);
            if (ec) {
                break;
            }

            auto destinationDir = "/" + targetDir.string();
            if (!std::filesystem::exists(destinationDir, ec)) {
                break;
            }

            mediaMount = {
                Mount{ .destination = destinationDir,
                       .options = string_list{ "rbind", "rshared" },
                       .source = destinationDir,
                       .type = "bind" },
                Mount{ .destination = "/media",
                       .options = string_list{ "rbind", "ro", "copy-symlink" },
                       .source = "/media",
                       .type = "bind" },
            };
        } else {
            mediaMount = {
                Mount{ .destination = "/media",
                       .options = string_list{ "rbind", "rshared" },
                       .source = "/media",
                       .type = "bind" },
            };
        }
    } while (false);

    return *this;
}

ContainerCfgBuilder &ContainerCfgBuilder::forwordDefaultEnv() noexcept
{
    return forwordEnv(std::vector<std::string>{
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
      "TERM" });
}

ContainerCfgBuilder &ContainerCfgBuilder::forwordEnv(std::vector<std::string> envList) noexcept
{
    envForword = envList;

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
    std::vector<std::string> statics{
        "/etc/machine-id",
        "/etc/resolvconf",
        "/etc/ssl/certs",
        "/usr/lib/locale",
        "/usr/share/fonts",
        "/usr/share/icons",
        "/usr/share/themes",
        "/usr/share/zoneinfo",
        "/var/cache/fontconfig",
        // TODO It's better to move to volatileMount
        "/etc/localtime",
        "/etc/resolv.conf",
        "/etc/timezone",
    };

    hostStaticsMount = std::vector<Mount>{};
    std::error_code ec;
    for (const auto &loc : statics) {
        if (!std::filesystem::exists(loc, ec)) {
            continue;
        }
        hostStaticsMount->emplace_back(Mount{ .destination = loc,
                                              .options = string_list{ "rbind", "ro" },
                                              .source = loc,
                                              .type = "bind" });
    }

    return *this;
}

ContainerCfgBuilder &ContainerCfgBuilder::bindHome(std::filesystem::path hostHome,
                                                   std::string user) noexcept
{
    homePath = hostHome;
    homeUser = user;
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

ContainerCfgBuilder &ContainerCfgBuilder::setExtraMounts(std::vector<Mount> extra) noexcept
{
    extraMount = extra;
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

bool ContainerCfgBuilder::checkValid() noexcept
{
    if (appId.empty()) {
        error_.reason = "app id is empty";
        error_.code = BUILD_PARAM_ERROR;
        return false;
    }

    if (!basePath) {
        error_.reason = "base path is not set";
        error_.code = BUILD_PARAM_ERROR;
        return false;
    }

    if (!bundlePath) {
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

    config.root = { .path = *basePath, .readonly = basePathRo };

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

    runtimeMount = Mount{ .destination = "/runtime",
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
                 Mount{ .destination = std::filesystem::path("/opt/apps") / appId / "files",
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

    if (homePath->empty() || homeUser.empty()) {
        error_.reason = "homePath or user is empty";
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

    auto containerHome = "/home/" + homeUser;

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

    value = getenv("XDG_CONFIG_HOME");
    std::filesystem::path XDG_CONFIG_HOME =
      value ? std::filesystem::path{ value } : *homePath / ".config";
    std::filesystem::path XDGConfigHome = XDG_CONFIG_HOME;
    auto privateConfigPath = privatePath / "config";
    if (std::filesystem::exists(privateConfigPath, ec)) {
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
    auto privateCachePath = privatePath / "cache";
    if (std::filesystem::exists(privateCachePath, ec)) {
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
    auto privateStatePath = privatePath / "state";
    if (std::filesystem::exists(privateStatePath, ec)) {
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

bool ContainerCfgBuilder::buildTmp() noexcept
{
    if (!tmpMount) {
        return true;
    }

    std::srand(std::time(0));
    auto tmpPath = std::filesystem::temp_directory_path() / "linglong"
      / (appId + "-" + std::to_string(std::rand()));
    std::error_code ec;
    if (!std::filesystem::create_directories(tmpPath, ec) && ec) {
        error_.reason = tmpPath.string() + "can't be created";
        error_.code = BUILD_MOUNT_TMP_ERROR;
        return false;
    }

    tmpMount = Mount{ .destination = "/tmp",
                      .options = string_list{ "rbind" },
                      .source = tmpPath,
                      .type = "bind" };

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

    auto bindIfExist = [this](std::string_view source, std::string_view destination) mutable {
        std::error_code ec;
        if (!std::filesystem::exists(source, ec)) {
            return;
        }

        auto realDest = destination.empty() ? source : destination;
        ipcMount->emplace_back(Mount{ .destination = std::string{ realDest },
                                      .options = string_list{ "rbind" },
                                      .source = std::string{ source },
                                      .type = "bind" });
    };

    bindIfExist("/tmp/.X11-unix", "");

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

    [this, &bindIfExist]() {
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
            std::cerr << hostXDGRuntimeDir << " doesn't belong to current user.";
            return;
        }

        auto cognitiveXDGRuntimeDir = std::filesystem::path{ environment["XDG_RUNTIME_DIR"] };

        bindIfExist((hostXDGRuntimeDir / "pulse").string(),
                    (cognitiveXDGRuntimeDir / "pulse").string());
        bindIfExist((hostXDGRuntimeDir / "gvfs").string(),
                    (cognitiveXDGRuntimeDir / "gvfs").string());

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
        auto cognitiveXauthFile = std::string{ "/home/" } + homeUser + "/.Xauthority";

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
    if (envForword) {
        for (const auto &key : *envForword) {
            auto *value = getenv(key.c_str());
            if (value) {
                environment[key] = value;
            }
        }
    }

    environment["LINGLONG_APPID"] = appId;

    auto envShFile = *bundlePath / "00env.sh";
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

bool ContainerCfgBuilder::mergeMount() noexcept
{
    // merge all mounts here, the order of mounts is relevant
    auto mounts = std::vector<Mount>{};

    if (runtimeMount) {
        mounts.insert(mounts.end(), std::move(*runtimeMount));
    }

    if (appMount) {
        std::move(appMount->begin(), appMount->end(), std::back_inserter(mounts));
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

    if (mediaMount) {
        std::move(mediaMount->begin(), mediaMount->end(), std::back_inserter(mounts));
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

    if (privateMount) {
        std::move(privateMount->begin(), privateMount->end(), std::back_inserter(mounts));
    }

    if (envMount) {
        mounts.insert(mounts.end(), std::move(*envMount));
    }

    if (extraMount) {
        std::move(extraMount->begin(), extraMount->end(), std::back_inserter(mounts));
    }

    if (selfAdjustingMountEnabled) {
        if (!selfAdjustingMount(mounts)) {
            return false;
        }
    }

    config.mounts = std::move(mounts);

    return true;
}

std::vector<Mount> ContainerCfgBuilder::generateMounts(const std::vector<MountNode> &mountpoints,
                                                       std::vector<Mount> &mounts) noexcept
{
    std::vector<Mount> generated;
    std::vector<int> nodes = { 0 };
    size_t idx = 0;

    while (nodes.size() > idx) {
        for (auto i : mountpoints[idx].childs_idx) {
            nodes.push_back(i);
            const auto &child = mountpoints[i];
            if (child.mount_idx >= 0) {
                generated.emplace_back(mounts[child.mount_idx]);
            }
        }
        ++idx;
    }

    return generated;
}

bool ContainerCfgBuilder::selfAdjustingMount(std::vector<Mount> &mounts) noexcept
{
    // Some apps depends on files which doesn't exist in runtime layer or base layer, we have to
    // mount host files to container, or create the file on demand, but the layer is readonly. We
    // make a workaround by mount the suitable target's ancestor directory as tmpfs.

    // mountpoints is a prefix tree of all mounts path
    // .mount_idx > 0 represents the path is a mount point, and it's the subscript of the array
    // mounts
    std::vector<MountNode> mountpoints;
    mountpoints.emplace_back(
      MountNode{ .name = "", .ro = false, .mount_idx = -1, .parent_idx = -1 });

    auto findChild = [&mountpoints](int parent, const std::string &name) {
        for (auto idx : mountpoints[parent].childs_idx) {
            if (mountpoints[idx].name == name) {
                return idx;
            }
        }

        return -1;
    };

    auto insertChild = [&mountpoints](int parent, MountNode &&node) -> int {
        node.parent_idx = parent;
        mountpoints.emplace_back(node);
        int child = mountpoints.size() - 1;
        mountpoints[parent].childs_idx.push_back(child);
        return child;
    };

    auto isRo = [](const Mount &mount) {
        // only try to adjust bind mount
        if (mount.type != "bind") {
            return false;
        }

        if (!mount.source) {
            return false;
        }

        // assume only /home/ and /tmp have write access
        if (mount.source->rfind("/home/", 0) == 0 || mount.source->rfind("/tmp/", 0) == 0) {
            return false;
        }

        return true;
    };

    auto findMountedParent = [&mountpoints](int child) {
        do {
            int parent = mountpoints[child].parent_idx;
            // root always hash mount point
            if (parent < 0) {
                return 0;
            }

            if (mountpoints[parent].mount_idx >= 0) {
                return parent;
            }
            child = parent;
        } while (true);

        return 0;
    };

    auto canBind = [&mountpoints, &mounts, findMountedParent, isRo, this](int node,
                                                                          std::string &failedPath) {
        int parent = findMountedParent(node);

        std::string root;
        if (parent == 0) {
            if (!mountpoints[0].ro) {
                return true;
            }
            root = config.root->path;
        } else {
            const auto &mountedParent = mounts[mountpoints[parent].mount_idx];
            if (!isRo(mountedParent)) {
                return true;
            }
            root = *mountedParent.source;
        }

        std::string path;
        int search = node;
        while (search != parent) {
            auto &mp = mountpoints[search];
            path = "/" + mp.name + path;
            search = mp.parent_idx;
        }

        std::error_code ec;
        if (std::filesystem::exists(root + path, ec)) {
            return true;
        }

        failedPath = root + path;

        return false;
    };

    auto getPath = [&mountpoints](int parent, int node) -> std::string {
        std::string path;
        while (node != 0) {
            const auto &mp = mountpoints[node];
            path = "/" + mp.name + path;
            if (node == parent) {
                break;
            }
            node = mp.parent_idx;
        }

        return path;
    };

    auto adjustNode = [&mountpoints, &mounts, getPath, findChild, insertChild](
                        int node,
                        std::filesystem::path path) {
        auto destination = getPath(0, node);

        // root will create in bundlePath/rootfs
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
            mp.ro = false;
        }

        for (auto const &entry : std::filesystem::directory_iterator{ path }) {
            auto filename = entry.path().filename();
            // if not bind, bind it and mark with fix flag
            if (findChild(node, filename) < 0) {
                auto source = path / filename;
                auto mount = Mount{ .destination = destination + "/" + filename.string(),
                                    .options = string_list{ "rbind" },
                                    .source = source,
                                    .type = "bind" };
                if (entry.is_symlink()) {
                    mount.options->emplace_back("copy-symlink");
                }
                mounts.emplace_back(std::move(mount));
                insertChild(node,
                            MountNode{ .name = filename,
                                       .ro = true,
                                       .fix = true,
                                       .mount_idx = static_cast<int>(mounts.size() - 1 )});
            }
        }
    };

    // adjust root first
    adjustNode(0, config.root->path);

    // change root path to bundlePath/rootfs
    auto rootfs = *bundlePath / "rootfs";
    std::error_code ec;
    if (!std::filesystem::create_directories(rootfs, ec) && ec) {
        error_.reason = rootfs.string() + " can't be created";
        error_.code = BUILD_PREPARE_ERROR;
        return false;
    }
    config.root = { .path = "rootfs", .readonly = true };

    for (size_t i = 0; i < mounts.size(); ++i) {
        auto &mount = mounts[i];

        // ignore empty type
        if (mount.type->empty()) {
            continue;
        }

        auto &destination = mount.destination;
        if (destination.empty() || destination[0] != '/') {
            error_.reason = destination + " as mount destination is invalid";
            error_.code = BUILD_MOUNT_ERROR;
            return false;
        }

        if (destination[destination.size() - 1] == '/') {
            destination.pop_back();
        }

        auto begin = destination.begin();
        auto it = ++begin;
        int find = 0;

        for (; it != destination.end();) {
            if (*it == '/') {
                auto name = std::string(begin, it);
                auto child = findChild(find, name);
                if (child >= 0) {
                    find = child;
                } else {
                    // insert path to mountpoints tree
                    find = insertChild(find,
                                       MountNode{
                                         .name = std::move(name),
                                         .ro = false,
                                         .fix = false,
                                         .mount_idx = -1,
                                       });
                }
                begin = ++it;
            } else {
                ++it;
            }
        }

        auto name = std::string(begin, it);
        auto child = findChild(find, name);
        // leaf node is a mount point
        if (child < 0) {
            child = insertChild(find,
                                MountNode{
                                  .name = std::move(name),
                                  .ro = isRo(mount),
                                  .fix = false,
                                  .mount_idx = static_cast<int>(i),
                                });
        } else {
            auto &mp = mountpoints[child];
            // if the mount point is created by adjustNode
            if (mp.fix) {
                if (mp.mount_idx != static_cast<int>(i)) {
                    // override the mount point created by adjustNode
                    // we also mask the mount point by set type to empty
                    mounts[mp.mount_idx].type = "";
                    mp.mount_idx = i;
                }
            }
        }

        std::string failedPath;
        // if current mount point can't be created
        if (!canBind(child, failedPath)) {
            std::filesystem::path path = { failedPath };

            int node = child;
            while (path.has_relative_path()) {
                path = path.parent_path();
                node = mountpoints[node].parent_idx;
                std::error_code ec;
                // find the nearest ancestor mount point can be modified
                if (std::filesystem::exists(path, ec)) {
                    adjustNode(node, path);
                    break;
                }
            }
        }
    }

    // use BFS to travel mountpoints tree to generate the mounts
    auto generated = generateMounts(mountpoints, mounts);
    mounts = std::move(generated);

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

    if (!buildMountHome() || !buildTmp() || !buildPrivateDir() || !buildPrivateMapped()) {
        return false;
    }

    if (!buildMountIPC()) {
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

    return true;
}

} // namespace linglong::generator
