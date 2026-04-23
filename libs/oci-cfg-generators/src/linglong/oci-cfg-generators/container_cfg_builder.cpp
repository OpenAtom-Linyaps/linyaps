/*
 * SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/oci-cfg-generators/container_cfg_builder.h"

#include "configure.h"
#include "linglong/api/types/v1/Generators.hpp"
#include "linglong/api/types/v1/OciConfigurationPatch.hpp"
#include "linglong/cdi/types/Generators.hpp"
#include "linglong/common/dir.h"
#include "linglong/common/display.h"
#include "linglong/common/strings.h"
#include "linglong/common/xdg.h"
#include "linglong/utils/cmd.h"
#include "linglong/utils/file.h"
#include "linglong/utils/log/log.h"
#include "linglong/utils/overlayfs.h"
#include "linglong/utils/sha256.h"
#include "ocppi/runtime/config/types/Generators.hpp"

#include <fmt/format.h>
#include <sys/mount.h>

#include <algorithm>
#include <climits>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

extern char **environ;

namespace linglong::generator {

using string_list = std::vector<std::string>;

#define BUILD_STEP(step)      \
    do {                      \
        auto result = step(); \
        if (!result) {        \
            return result;    \
        }                     \
    } while (0)

using ocppi::runtime::config::types::Capabilities;
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

struct DBusAddress
{
    std::string transport;
    std::map<std::string, std::string> options;
};

std::vector<DBusAddress> parseDBusAddressForMount(std::string_view address) noexcept
{
    // ref: https://dbus.freedesktop.org/doc/dbus-specification.html#addresses
    std::vector<DBusAddress> ret;

    auto addresses = common::strings::split(address, ';', common::strings::splitOption::SkipEmpty);
    for (auto addr : addresses) {
        auto colonPos = addr.find(':');
        if (colonPos == std::string_view::npos) {
            continue;
        }

        DBusAddress address;
        address.transport = addr.substr(0, colonPos);

        auto optionsStr = addr.substr(colonPos + 1);
        auto options =
          common::strings::split(optionsStr, ',', common::strings::splitOption::SkipEmpty);
        for (auto option : options) {
            auto equalPos = option.find('=');
            if (equalPos == std::string_view::npos) {
                address.options.insert_or_assign(std::string(option), "");
                continue;
            }

            auto key = option.substr(0, equalPos);
            auto value = option.substr(equalPos + 1);
            auto urlDecoded = common::strings::decode_url(value);
            if (!urlDecoded) {
                std::cerr << "dbus address option is invalid:" << value << std::endl;
                continue;
            }

            address.options.insert_or_assign(std::string(key), std::move(urlDecoded).value());
        }

        ret.emplace_back(std::move(address));
    }

    return ret;
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
    case ANNOTATION::WAYLAND_SOCKET:
        config.annotations->insert_or_assign("cn.org.linyaps.runtime.ws.path", std::move(value));
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
    bindRun();

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
                       .source = "proc",
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
            if (name == "snd" || name == "dri" || name == "jmgpu" || name.rfind("video", 0) == 0
                || name.rfind("nvidia", 0) == 0) {
                return true;
            }
            return false;
        };
    }

    devNodeMount = std::vector<Mount>{};
    for (const auto &entry : std::filesystem::directory_iterator{ "/dev" }) {
        if (ifBind(entry.path().filename())) {
            Mount m{ .destination = entry.path(),
                     .options = string_list{ "rbind" },
                     .source = entry.path(),
                     .type = "bind" };

            devNodeMount->emplace_back(std::move(m));
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
    runMount = { Mount{ .destination = "/run",
                        .options = string_list{ "nosuid", "nodev", "mode=0755", "size=65536k" },
                        .source = "tmpfs",
                        .type = "tmpfs" } };

    return *this;
}

ContainerCfgBuilder &ContainerCfgBuilder::bindXDGRuntime() noexcept
{
    containerXDGRuntimeDir = std::filesystem::path{ "/run/user" } / std::to_string(::geteuid());

    return *this;
}

utils::error::Result<void> ContainerCfgBuilder::buildXDGRuntime() noexcept
{
    LINGLONG_TRACE("build XDG runtime directory");

    if (!containerXDGRuntimeDir) {
        return LINGLONG_OK;
    }

    if (!runMount) {
        return LINGLONG_ERR("/run is not bind");
    }

    auto hostXDGRuntimeMountPoint = common::dir::getAppRuntimeDir(appId);
    std::error_code ec;
    std::filesystem::create_directories(hostXDGRuntimeMountPoint, ec);
    if (ec) {
        return LINGLONG_ERR("failed to create directories for container XDG_RUNTIME_DIR", ec);
    }

    std::filesystem::permissions(hostXDGRuntimeMountPoint, std::filesystem::perms::owner_all, ec);
    if (ec) {
        return LINGLONG_ERR("failed to set permissions for container XDG_RUNTIME_DIR", ec);
    }

    runMount->emplace_back(Mount{ .destination = *containerXDGRuntimeDir,
                                  .options = string_list{ "bind" },
                                  .source = hostXDGRuntimeMountPoint,
                                  .type = "bind" });
    environment["XDG_RUNTIME_DIR"] = *containerXDGRuntimeDir;

    return LINGLONG_OK;
}

ContainerCfgBuilder &ContainerCfgBuilder::bindTmp() noexcept
{
    tmpMount = Mount{
        .destination = "/tmp",
        .options = string_list{ "rbind" },
        .source = "/tmp",
        .type = "bind",
    };

    isolateTmp = false;

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

    std::vector<std::string> propagationPaths{ "/media", "/run/media", "/mnt" };
    removableStorageMounts = std::vector<Mount>{};

    for (const auto &path : propagationPaths) {
        bindIfExist(*removableStorageMounts, path, path, false);
    }

    return *this;
}

ContainerCfgBuilder &ContainerCfgBuilder::forwardDefaultEnv() noexcept
{
    return forwardEnv(std::vector<std::string>{
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
      "CLUTTER_IM_MODULE",
      "QT4_IM_MODULE",
      "GTK_IM_MODULE",
      "all_proxy",
      "auto_proxy",    // 网络系统代理自动代理
      "http_proxy",    // 网络系统代理手动http代理
      "https_proxy",   // 网络系统代理手动https代理
      "ftp_proxy",     // 网络系统代理手动ftp代理
      "SOCKS_SERVER",  // 网络系统代理手动socks代理
      "no_proxy",      // 网络系统代理手动配置代理
      "USER",          // wine应用会读取此环境变量
      "QT_IM_MODULE",  // 输入法
      "LINGLONG_ROOT", // 玲珑安装位置
      "QT_QPA_PLATFORM",
      "QT_WAYLAND_SHELL_INTEGRATION",
      "GDMSESSION",
      "QT_WAYLAND_FORCE_DPI",
      "GIO_LAUNCHED_DESKTOP_FILE", // 系统监视器
      "GNOME_DESKTOP_SESSION_ID",  // gnome 桌面标识，有些应用会读取此变量以使用gsettings配置,
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

ContainerCfgBuilder &ContainerCfgBuilder::enableLDConf() noexcept
{
    ldConfMount = std::vector<Mount>{};
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
    startContainerHooks = hooks;
    return *this;
}

ContainerCfgBuilder &ContainerCfgBuilder::addExtraHook(const std::string &type, Hook hook) noexcept
{
    if (!extraHooks) {
        extraHooks = std::vector<std::pair<std::string, Hook>>{};
    }

    extraHooks->emplace_back(std::make_pair(type, std::move(hook)));

    return *this;
}

utils::error::Result<void> ContainerCfgBuilder::buildHooks() noexcept
{
    if (startContainerHooks || extraHooks) {
        config.hooks = Hooks{};

        if (startContainerHooks) {
            config.hooks->startContainer = startContainerHooks;
        }

        if (extraHooks) {
            for (auto &[type, hook] : *extraHooks) {
                std::optional<std::vector<Hook>> *phook = nullptr;
                if (type == "createContainer") {
                    phook = &config.hooks->createContainer;
                } else if (type == "createRuntime") {
                    phook = &config.hooks->createRuntime;
                } else if (type == "poststart") {
                    phook = &config.hooks->poststart;
                } else if (type == "poststop") {
                    phook = &config.hooks->poststop;
                } else if (type == "prestart") {
                    phook = &config.hooks->prestart;
                } else if (type == "startContainer") {
                    phook = &config.hooks->startContainer;
                } else {
                    continue;
                }

                if (!phook->has_value()) {
                    *phook = std::vector<Hook>{};
                }

                phook->value().emplace_back(std::move(hook));
            }
        }
    }

    return LINGLONG_OK;
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

utils::error::Result<void> ContainerCfgBuilder::checkValid() noexcept
{
    LINGLONG_TRACE("check validation");

    if (appId.empty()) {
        return LINGLONG_ERR("app id is empty");
    }

    if (basePath.empty()) {
        return LINGLONG_ERR("base path is not set");
    }

    if (bundlePath.empty()) {
        return LINGLONG_ERR("bundle path is empty");
    }

    if (overlayMerged) {
        if (selfAdjustingMountEnabled) {
            return LINGLONG_ERR(
              "overlay and self-adjusting mount cannot be enabled simultaneously");
        }
    }

    return LINGLONG_OK;
}

utils::error::Result<void> ContainerCfgBuilder::prepare() noexcept
{
    LINGLONG_TRACE("prepare container configuration");

    config.ociVersion = "1.0.1";
    config.hostname = "linglong";

    auto linux_ = ocppi::runtime::config::types::Linux{};
    linux_.rootfsPropagation = RootfsPropagation::Slave;
    linux_.namespaces = std::vector<NamespaceReference>{
        NamespaceReference{ .type = NamespaceType::Pid },
        NamespaceReference{ .type = NamespaceType::Mount },
        NamespaceReference{ .type = NamespaceType::Uts },
    };
    if (!disableUserNamespaceEnabled) {
        linux_.namespaces->push_back(NamespaceReference{ .type = NamespaceType::User });
    }
    if (isolateNetWorkEnabled) {
        linux_.namespaces->push_back(NamespaceReference{ .type = NamespaceType::Network });
    }
    config.linux_ = std::move(linux_);

    auto process = Process{ .args = string_list{ "bash" }, .cwd = "/" };
    if (capabilities) {
        process.capabilities = Capabilities{
            .bounding = capabilities,
            .effective = capabilities,
            .permitted = capabilities,
        };
    }
    config.process = std::move(process);

    config.root = { .path = overlayMerged ? overlayMerged->first : basePath,
                    .readonly = overlayMerged ? overlayMerged->second : basePathRo };

    return LINGLONG_OK;
}

utils::error::Result<void> ContainerCfgBuilder::buildIdMappings() noexcept
{
    LINGLONG_TRACE("build ID mappings");

    config.linux_->uidMappings = std::move(uidMappings);
    config.linux_->gidMappings = std::move(gidMappings);

    return LINGLONG_OK;
}

ContainerCfgBuilder &ContainerCfgBuilder::setContainerId(std::string containerId) noexcept
{
    this->containerId = std::move(containerId);
    return *this;
}

utils::error::Result<void> ContainerCfgBuilder::buildMountRuntime() noexcept
{
    LINGLONG_TRACE("build runtime mount");

    if (!runtimePath) {
        return LINGLONG_OK;
    }

    std::error_code ec;
    if (!std::filesystem::exists(*runtimePath, ec)) {
        return LINGLONG_ERR("runtime files is not exist", ec);
    }

    runtimeMount = Mount{ .destination = runtimeMountPoint,
                          .options = string_list{ "rbind", runtimePathRo ? "ro" : "rw" },
                          .source = *runtimePath,
                          .type = "bind" };

    return LINGLONG_OK;
}

utils::error::Result<void> ContainerCfgBuilder::buildMountApp() noexcept
{
    LINGLONG_TRACE("build app mount");

    if (!appPath) {
        return LINGLONG_OK;
    }

    std::error_code ec;
    if (!std::filesystem::exists(*appPath, ec)) {
        return LINGLONG_ERR("app files is not exist", ec);
    }

    appMount = { Mount{ .destination = "/opt",
                        .options = string_list{ "nodev", "nosuid", "mode=700" },
                        .source = "tmpfs",
                        .type = "tmpfs" },
                 Mount{ .destination = std::filesystem::path{ "/opt/apps" } / appId / "files",
                        .options = string_list{ "rbind", appPathRo ? "ro" : "rw" },
                        .source = *appPath,
                        .type = "bind" } };

    return LINGLONG_OK;
}

utils::error::Result<void> ContainerCfgBuilder::buildMountHome() noexcept
{
    LINGLONG_TRACE("build home mount");

    if (!homePath) {
        return LINGLONG_OK;
    }

    if (homePath->empty()) {
        return LINGLONG_ERR("homePath is empty");
    }

    std::error_code ec;
    if (!std::filesystem::exists(*homePath, ec)) {
        return LINGLONG_ERR(fmt::format("{} is not exist", *homePath), ec);
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

    auto mountDir = [this](const std::filesystem::path &hostDir,
                           const std::string &containerDir) -> bool {
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
            return LINGLONG_ERR(fmt::format("{} can't be mount", XDG_DATA_HOME));
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
            return LINGLONG_ERR(fmt::format("{} can't be mount", XDGConfigHome));
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
            return LINGLONG_ERR(fmt::format("{} can't be mount", XDGCacheHome));
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
            return LINGLONG_ERR(fmt::format("{} can't be mount", XDG_STATE_HOME));
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

    return LINGLONG_OK;
}

utils::error::Result<void> ContainerCfgBuilder::buildPrivateDir() noexcept
{
    LINGLONG_TRACE("build private directory");

    if (!privateMount) {
        return LINGLONG_OK;
    }

    if (!homePath) {
        return LINGLONG_ERR("must bind home first");
    }

    privatePath = *homePath / ".linglong";
    privateAppDir = privatePath / appId;
    std::error_code ec;
    if (!std::filesystem::create_directories(privateAppDir, ec) && ec) {
        return LINGLONG_ERR(fmt::format("{} can't be created", privateAppDir));
    }

    // hide private directory
    maskedPaths.emplace_back(privatePath);

    return LINGLONG_OK;
}

utils::error::Result<void> ContainerCfgBuilder::buildPrivateMapped() noexcept
{
    LINGLONG_TRACE("build private mapped");

    if (!privateMappings) {
        return LINGLONG_OK;
    }

    if (!privateMount) {
        return LINGLONG_ERR("must enable private dir first");
    }

    for (const auto &[path, isDir] : *privateMappings) {
        std::filesystem::path containerPath{ path };
        if (!containerPath.is_absolute()) {
            return LINGLONG_ERR("must pass absolute path in container");
        }

        std::filesystem::path hostPath =
          privateAppDir / "private" / containerPath.lexically_relative("/");

        std::error_code ec;
        if (isDir) {
            // always create directory
            if (!std::filesystem::create_directories(hostPath, ec) && ec) {
                return LINGLONG_ERR(fmt::format("{} can't be created", hostPath), ec);
            }
        }
        if (!std::filesystem::exists(hostPath, ec)) {
            return LINGLONG_ERR(fmt::format("mapped private path {} is not exist", hostPath), ec);
        }

        privateMount->emplace_back(Mount{ .destination = containerPath,
                                          .options = string_list{ "rbind" },
                                          .source = hostPath,
                                          .type = "bind" });
    }

    return LINGLONG_OK;
}

ContainerCfgBuilder &
ContainerCfgBuilder::bindXAuthFile(const std::filesystem::path &authFile) noexcept
{
    xAuthFile = authFile;
    return *this;
}

ContainerCfgBuilder &
ContainerCfgBuilder::bindWaylandSocket(const std::filesystem::path &socket) noexcept
{
    waylandSocket = socket;
    return *this;
}

utils::error::Result<void> ContainerCfgBuilder::buildDisplaySystem() noexcept
{
    LINGLONG_TRACE("build display system");

    displayMount = std::vector<Mount>{};

    if (auto *display = ::getenv("DISPLAY"); display != nullptr) {
        auto xOrgDisplayConf = common::display::getXOrgDisplay(display);
        if (xOrgDisplayConf) {
            std::filesystem::path source;
            int displayNo = 0;
            if (xOrgDisplayConf->protocol && xOrgDisplayConf->protocol == "unix"
                && xOrgDisplayConf->host && !xOrgDisplayConf->host->empty()
                && xOrgDisplayConf->host->at(0) == '/') {
                // xcb may use abstract socket, so we use a different display number
                displayNo = 1000;
                source = *xOrgDisplayConf->host;

                if (xOrgDisplayConf->screenNo != 0) {
                    environment["DISPLAY"] =
                      fmt::format(":{}.{}", displayNo, xOrgDisplayConf->screenNo);
                } else {
                    environment["DISPLAY"] = fmt::format(":{}", displayNo);
                }
            } else {
                displayNo = xOrgDisplayConf->displayNo;
                source = fmt::format("/tmp/.X11-unix/X{}", displayNo);
                environment["DISPLAY"] = display;
            }

            displayMount->emplace_back(
              Mount{ .destination = "/tmp/.X11-unix",
                     .options = string_list{ "nodev", "nosuid", "mode=700" },
                     .source = "tmpfs",
                     .type = "tmpfs" });

            std::error_code ec;
            if (std::filesystem::exists(source, ec)) {
                displayMount->emplace_back(
                  Mount{ .destination = fmt::format("/tmp/.X11-unix/X{}", displayNo),
                         .options = string_list{ "bind" },
                         .source = source,
                         .type = "bind" });
            }
        }
    }

    if (xAuthFile) {
        if (!runMount) {
            return LINGLONG_ERR("must enable run mount first");
        }

        auto xAuthPath = "/run/linglong/Xauthority";
        displayMount->emplace_back(Mount{ .destination = xAuthPath,
                                          .options = string_list{ "bind" },
                                          .source = xAuthFile.value(),
                                          .type = "bind" });
        environment["XAUTHORITY"] = xAuthPath;
    }

    if (waylandSocket) {
        auto waylandSocketPath = containerXDGRuntimeDir
          ? (*containerXDGRuntimeDir / "wayland-0")
          : std::filesystem::path{ "/run/linglong/wayland-0" };
        displayMount->emplace_back(Mount{ .destination = waylandSocketPath,
                                          .options = string_list{ "bind" },
                                          .source = waylandSocket.value(),
                                          .type = "bind" });
        environment["WAYLAND_DISPLAY"] = waylandSocketPath;
    }

    return LINGLONG_OK;
}

utils::error::Result<void> ContainerCfgBuilder::buildMountIPC() noexcept
{
    LINGLONG_TRACE("build IPC mount");

    if (!ipcMount) {
        return LINGLONG_OK;
    }

    if (!containerXDGRuntimeDir) {
        return LINGLONG_ERR("must enable xdg runtime mount first");
    }

    auto bindDbusBus = [this](const std::string &envName,
                              const std::filesystem::path &containerDest,
                              const std::string_view defaultAddr = "") {
        auto rawAddr = defaultAddr;
        if (auto *cStr = ::getenv(envName.c_str()); cStr != nullptr) {
            rawAddr = cStr;
        }

        if (rawAddr.empty()) {
            return;
        }

        auto addresses = parseDBusAddressForMount(rawAddr);

        std::string selectedSource;
        std::vector<std::pair<std::string, std::string>> otherOptions;

        for (const auto &addr : addresses) {
            if (addr.transport != "unix") {
                continue;
            }

            auto it = addr.options.find("path");
            if (it != addr.options.cend() && !it->second.empty()) {
                std::error_code ec;
                if (std::filesystem::exists(it->second, ec)) {
                    selectedSource = it->second;

                    for (const auto &[key, val] : addr.options) {
                        if (key != "path") {
                            otherOptions.emplace_back(key, val);
                        }
                    }

                    break;
                }
            }
        }

        if (selectedSource.empty()) {
            std::cerr << "No valid unix socket found for " << envName << std::endl;
            return;
        }

        ipcMount->emplace_back(Mount{ .destination = containerDest.string(),
                                      .options = std::vector<std::string>{ "rbind" },
                                      .source = selectedSource,
                                      .type = "bind" });

        auto newEnv = fmt::format("unix:path={}", containerDest.string());
        for (const auto &[key, val] : otherOptions) {
            fmt::format_to(std::back_inserter(newEnv),
                           ",{}={}",
                           key,
                           common::strings::encode_url(val));
        }

        environment[envName] = std::move(newEnv);
    };

    [this, &bindDbusBus]() {
        // System Bus
        bindDbusBus("DBUS_SYSTEM_BUS_ADDRESS",
                    "/run/dbus/system_bus_socket",
                    "unix:path=/var/run/dbus/system_bus_socket");

        // Session Bus
        if (!containerXDGRuntimeDir->empty()) {
            bindDbusBus("DBUS_SESSION_BUS_ADDRESS", *containerXDGRuntimeDir / "bus");
        } else {
            std::cerr << "XDG_RUNTIME_DIR not set, skipping session bus mount." << std::endl;
        }
    }();

    auto hostXDGRuntimeDir = common::xdg::getXDGRuntimeDir();

    bindIfExist(*ipcMount,
                hostXDGRuntimeDir / "pulse",
                (*containerXDGRuntimeDir / "pulse").string(),
                false);

    bindIfExist(*ipcMount,
                hostXDGRuntimeDir / "gvfs",
                (*containerXDGRuntimeDir / "gvfs").string(),
                false);

    [this, &hostXDGRuntimeDir]() {
        auto dconfPath = std::filesystem::path(hostXDGRuntimeDir) / "dconf";
        if (!std::filesystem::exists(dconfPath)) {
            std::cerr << "dconf directory not found at " << dconfPath << "." << std::endl;
            return;
        }

        ipcMount->emplace_back(ocppi::runtime::config::types::Mount{
          .destination = *containerXDGRuntimeDir / "dconf",
          .options = string_list{ "rbind" },
          .source = dconfPath.string(),
          .type = "bind",
        });
    }();

    return LINGLONG_OK;
}

utils::error::Result<void> ContainerCfgBuilder::buildMountCache() noexcept
{
    LINGLONG_TRACE("build cache mount");

    if (!appCache) {
        return LINGLONG_OK;
    }

    std::error_code ec;
    if (!std::filesystem::exists(*appCache, ec)) {
        return LINGLONG_ERR(fmt::format("app cache {} does not exist", *appCache), ec);
    }

    cacheMount = { Mount{ .destination = "/run/linglong/cache",
                          .options = string_list{ "rbind", appCacheRo ? "ro" : "rw" },
                          .source = *appCache,
                          .type = "bind" } };

    return LINGLONG_OK;
}

utils::error::Result<void> ContainerCfgBuilder::buildLDCache() noexcept
{
    LINGLONG_TRACE("build LD cache");

    if (!ldCacheMount && !ldConfMount) {
        return LINGLONG_OK;
    }

    if (!appCache) {
        return LINGLONG_ERR("app cache is not set");
    }

    if (ldCacheMount) {
        ldCacheMount->emplace_back(Mount{ .destination = "/etc/ld.so.cache",
                                          .options = string_list{ "rbind", "ro" },
                                          .source = *appCache / "ld.so.cache",
                                          .type = "bind" });
    }

    if (ldConfMount) {
        ldConfMount->emplace_back(
          Mount{ .destination = "/etc/ld.so.conf.d/zz_deepin-linglong-app.conf",
                 .options = { { "rbind", "ro" } },
                 .source = *appCache / "ld.so.conf",
                 .type = "bind" });
    }

    return LINGLONG_OK;
}

utils::error::Result<void> ContainerCfgBuilder::buildMountTimeZone() noexcept
{
    LINGLONG_TRACE("build local time mount");

    timeZoneMount = std::vector<Mount>{};

    auto *tzdir_env = getenv("TZDIR");
    std::filesystem::path tzdir{ "/usr/share/zoneinfo" };
    if (tzdir_env != nullptr && tzdir_env[0] != '\0') {
        tzdir = tzdir_env;
    }
    bindIfExist(*timeZoneMount, tzdir, zoneinfoMountPoint);
    bindIfExist(*timeZoneMount, "/etc/timezone");

    if (timezone && timezone->empty()) {
        LogW("timezone not set, bind host /etc/localtime");
        bindIfExist(*timeZoneMount, "/etc/localtime");
    }

    return LINGLONG_OK;
}

utils::error::Result<void> ContainerCfgBuilder::buildMountNetworkConf() noexcept
{
    LINGLONG_TRACE("build network configuration mount");

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
                    return LINGLONG_ERR(
                      fmt::format("Failed to read symlink {}: {}", resolvConf, strerror(errno)));
                }
                target = std::filesystem::path{ rpath };
            }

            target = std::filesystem::path{ "/run/host/rootfs" } / target.lexically_relative("/");
            auto bundleResolvConf = bundlePath / "resolv.conf";
            std::filesystem::create_symlink(target, bundleResolvConf, ec);
            if (ec) {
                return LINGLONG_ERR(fmt::format("Failed to create symlink {}", bundleResolvConf),
                                    ec);
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
    return LINGLONG_OK;
}

// TODO
utils::error::Result<void> ContainerCfgBuilder::buildQuirkVolatile() noexcept
{
    LINGLONG_TRACE("build quirk volatile");

    if (!volatileMount) {
        return LINGLONG_OK;
    }

    if (!hostRootMount) {
        return LINGLONG_ERR("/run/host/rootfs must mount first");
    }

    return LINGLONG_ERR("TODO: buildQuirkVolatile not implemented");
}

utils::error::Result<void> ContainerCfgBuilder::buildEnv() noexcept
{
    LINGLONG_TRACE("build environment");

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
        return LINGLONG_ERR(fmt::format("{} can't be created", envShFile));
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

    return LINGLONG_OK;
}

ContainerCfgBuilder &ContainerCfgBuilder::disableContainerInfo() noexcept
{
    disableGenerateContainerInfo = true;
    return *this;
}

utils::error::Result<void> ContainerCfgBuilder::buildContainerInfo() noexcept
{
    LINGLONG_TRACE("build container info");

    if (disableGenerateContainerInfo) {
        return LINGLONG_OK;
    }

    const auto *iniTemplate = R"([General]
Linyaps-version={}

[Application]
Id={}

[Instance]
Id={}

[Context]
Network={}

)";

    const auto content = fmt::format(iniTemplate,
                                     LINGLONG_VERSION,
                                     appId,
                                     containerId,
                                     isolateNetWorkEnabled ? "unshared" : "shared");
    auto containerInfoFile = bundlePath / ".linyaps";

    {
        std::ofstream ofs(containerInfoFile);
        if (!ofs.is_open()) {
            return LINGLONG_ERR(fmt::format("{} can't be created", containerInfoFile));
        }

        ofs << content;
    }

    infoMount = Mount{ .destination = "/.linyaps",
                       .options = string_list{ "rbind", "ro" },
                       .source = containerInfoFile,
                       .type = "bind" };
    return LINGLONG_OK;
}

utils::error::Result<void> ContainerCfgBuilder::applyPatch() noexcept
{
    LINGLONG_TRACE("apply patches");

    if (!applyPatchEnabled) {
        return LINGLONG_OK;
    }

    std::filesystem::path containerConfigPath{ LINGLONG_INSTALL_PREFIX
                                               "/lib/linglong/container/config.d" };
    std::error_code ec;
    if (!std::filesystem::exists(containerConfigPath, ec)) {
        // if no-exists or failed to check exists, ignore it
        return LINGLONG_OK;
    }

    std::vector<std::filesystem::path> globalPatchFiles;
    std::vector<std::filesystem::path> appPatchFiles;
    auto iter = std::filesystem::directory_iterator{
        containerConfigPath,
        std::filesystem::directory_options::skip_permission_denied,
        ec
    };
    if (ec) {
        return LINGLONG_ERR(fmt::format("failed to iterate directory {}", containerConfigPath), ec);
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
                    return LINGLONG_ERR(fmt::format("failed to iterate directory {}", path), ec);
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

    auto doPatch = [this](const std::vector<std::filesystem::path> &patchFiles) {
        for (const auto &patchFile : patchFiles) {
            // skip if failed to apply
            auto result = applyPatchFile(patchFile);
            if (!result) {
                std::cerr << "skip applying failed patch " << patchFile << ": "
                          << result.error().message() << std::endl;
            }
        }
    };

    doPatch(globalPatchFiles);
    doPatch(appPatchFiles);

    return LINGLONG_OK;
}

utils::error::Result<void>
ContainerCfgBuilder::applyPatchFile(const std::filesystem::path &patchFile) noexcept
{
    LINGLONG_TRACE(fmt::format("apply patch file: {}", patchFile));

    std::error_code ec;
    auto status = std::filesystem::status(patchFile, ec);
    if (ec) {
        return LINGLONG_ERR(fmt::format("Failed to get status of patch file {}", patchFile), ec);
    }

    if ((status.permissions() & std::filesystem::perms::owner_exec) != std::filesystem::perms::none
        || (status.permissions() & std::filesystem::perms::group_exec)
          != std::filesystem::perms::none
        || (status.permissions() & std::filesystem::perms::others_exec)
          != std::filesystem::perms::none) {
        return applyExecutablePatch(patchFile);
    }

    if (patchFile.extension() == ".json") {
        return applyJsonPatchFile(patchFile);
    }

    return LINGLONG_ERR("Patch file is not an executable or a JSON patch file");
}

utils::error::Result<void>
ContainerCfgBuilder::applyJsonPatchFile(const std::filesystem::path &patchFile) noexcept
{
    LINGLONG_TRACE(fmt::format("apply JSON patch file: {}", patchFile));

    std::ifstream file(patchFile);
    if (!file.is_open()) {
        return LINGLONG_ERR(fmt::format("Failed to open file {}", patchFile));
    }

    try {
        auto json = nlohmann::json::parse(file);
        auto patchContent = json.get<linglong::api::types::v1::OciConfigurationPatch>();

        if (config.ociVersion != patchContent.ociVersion) {
            return LINGLONG_ERR("ociVersion mismatched");
        }

        auto raw = nlohmann::json(config);
        auto patchedJson = raw.patch(patchContent.patch);
        config = patchedJson.get<Config>();
    } catch (const std::exception &e) {
        return LINGLONG_ERR(fmt::format("Failed to apply JSON patch {}", patchFile), e);
    }

    return LINGLONG_OK;
}

utils::error::Result<void>
ContainerCfgBuilder::applyExecutablePatch(const std::filesystem::path &patchFile) noexcept
{
    LINGLONG_TRACE(fmt::format("apply executable patch: {}", patchFile));

    std::string inputJsonStr;
    try {
        inputJsonStr = nlohmann::json(config).dump();
    } catch (const std::exception &e) {
        return LINGLONG_ERR("Failed to serialize config", e);
    }

    auto output = utils::Cmd(patchFile.string()).toStdin(std::move(inputJsonStr)).exec();
    if (!output) {
        return LINGLONG_ERR(fmt::format("Failed to execute patch {}", patchFile), output.error());
    }

    try {
        config = nlohmann::json::parse(*output).get<Config>();
    } catch (const std::exception &e) {
        return LINGLONG_ERR(fmt::format("Failed to process output from {}: {}. Output: {}",
                                        patchFile.string(),
                                        e.what(),
                                        *output));
    }
    return LINGLONG_OK;
}

utils::error::Result<void>
ContainerCfgBuilder::applyCDIPatch(const linglong::cdi::types::ContainerEdits &edits) noexcept
{
    if (edits.env) {
        for (auto &e : *edits.env) {
            auto delimiter = e.find('=');
            if (delimiter != std::string::npos) {
                appendEnv(e.substr(0, delimiter), e.substr(delimiter + 1));
            }
        }
    }

    if (edits.deviceNodes) {
        if (!devNodeMount) {
            devNodeMount = std::vector<Mount>{};
        }
        for (auto &d : *edits.deviceNodes) {
            Mount mount{ .destination = d.path,
                         .options = string_list{ "bind" },
                         .source = d.hostPath.has_value() ? *d.hostPath : d.path,
                         .type = "bind" };

            devNodeMount->emplace_back(std::move(mount));
        }
    }

    if (edits.mounts) {
        auto mountType = [](const cdi::types::Mount &m) -> std::string {
            if (m.type) {
                return m.type.value();
            }

            if (m.options) {
                auto it =
                  std::find_if(m.options->begin(), m.options->end(), [](const std::string &option) {
                      return option == "bind" || option == "rbind";
                  });
                if (it != m.options->end()) {
                    return "bind";
                }
            }

            return "none";
        };

        for (auto &m : *edits.mounts) {
            Mount mount{ .destination = m.containerPath,
                         .options = m.options,
                         .source = m.hostPath,
                         .type = mountType(m) };

            addExtraMount(std::move(mount));
        }
    }

    if (edits.hooks) {
        for (auto &h : *edits.hooks) {
            addExtraHook(
              h.hookName,
              Hook{ .args = h.args, .env = h.env, .path = h.path, .timeout = h.timeout });
        }
    }

    return LINGLONG_OK;
}

utils::error::Result<void> ContainerCfgBuilder::mergeMount() noexcept
{
    LINGLONG_TRACE("merge all mounts");

    // merge all mounts here, the order of mounts is relevant
    if (runtimeMount) {
        mounts.emplace_back(std::move(runtimeMount).value());
    }

    if (appMount) {
        std::move(appMount->begin(), appMount->end(), std::back_inserter(mounts));
    }

    if (extensionMount) {
        std::copy(extensionMount->begin(), extensionMount->end(), std::back_inserter(mounts));
    }

    if (sysMount) {
        mounts.insert(mounts.end(), std::move(sysMount).value());
    }

    if (procMount) {
        mounts.insert(mounts.end(), std::move(procMount).value());
    }

    if (devMount) {
        std::move(devMount->begin(), devMount->end(), std::back_inserter(mounts));
    }

    if (devNodeMount) {
        std::move(devNodeMount->begin(), devNodeMount->end(), std::back_inserter(mounts));
    }

    if (cgroupMount) {
        mounts.insert(mounts.end(), std::move(cgroupMount).value());
    }

    if (runMount) {
        std::move(runMount->begin(), runMount->end(), std::back_inserter(mounts));
    }

    if (tmpMount) {
        mounts.insert(mounts.end(), std::move(tmpMount).value());
    }

    if (UGMount) {
        std::move(UGMount->begin(), UGMount->end(), std::back_inserter(mounts));
    }

    if (removableStorageMounts) {
        std::move(removableStorageMounts->begin(),
                  removableStorageMounts->end(),
                  std::back_inserter(mounts));
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

    if (displayMount) {
        std::move(displayMount->begin(), displayMount->end(), std::back_inserter(mounts));
    }

    if (infoMount) {
        mounts.emplace_back(std::move(infoMount).value());
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

    if (ldConfMount) {
        std::move(ldConfMount->begin(), ldConfMount->end(), std::back_inserter(mounts));
    }

    if (timeZoneMount) {
        std::move(timeZoneMount->begin(), timeZoneMount->end(), std::back_inserter(mounts));
    }

    if (networkConfMount) {
        std::move(networkConfMount->begin(), networkConfMount->end(), std::back_inserter(mounts));
    }

    if (privateMount) {
        std::move(privateMount->begin(), privateMount->end(), std::back_inserter(mounts));
    }

    if (envMount) {
        mounts.emplace_back(std::move(envMount).value());
    }

    if (extraMount) {
        std::move(extraMount->begin(), extraMount->end(), std::back_inserter(mounts));
    }

    config.mounts = std::move(mounts);

    return LINGLONG_OK;
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

void ContainerCfgBuilder::adjustNode(int node,
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
}

utils::error::Result<void> ContainerCfgBuilder::constructMountpointsTree() noexcept
{
    LINGLONG_TRACE("construct mountpoints tree");

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
        if (destination.empty()) {
            return LINGLONG_ERR(fmt::format("empty mount destination is invalid, source is {}",
                                            mount.source.value_or("[no source]")));
        }

        if (!destination.is_absolute()) {
            return LINGLONG_ERR(
              fmt::format("{} as mount destination is invalid", destination.string()));
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

    return LINGLONG_OK;
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

    // Traverse the nodes to be processed in reverse order to ensure child nodes are handled
    // before their parent nodes.
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

utils::error::Result<void> ContainerCfgBuilder::selfAdjustingMount() noexcept
{
    LINGLONG_TRACE("self adjusting mount");

    if (!selfAdjustingMountEnabled) {
        return LINGLONG_OK;
    }

    mounts = std::move(config.mounts).value();

    // Some apps depends on files which doesn't exist in runtime layer or base layer, we have to
    // mount host files to container, or create the file on demand, but the layer is readonly.
    // We make a workaround by mount the suitable target's ancestor directory as tmpfs.
    auto result = constructMountpointsTree();
    if (!result) {
        return result;
    }

    // Remounting as tmpfs requires an alternate rootfs context to avoid obscuring underlying
    // files, so adjust root and change root path to bundlePath/rootfs
    adjustNode(0, config.root->path, "");
    config.root->path = "rootfs";

    tryFixMountpointsTree();

    generateMounts();

    config.mounts = std::move(mounts);

    return LINGLONG_OK;
}

utils::error::Result<void> ContainerCfgBuilder::finalize() noexcept
{
    LINGLONG_TRACE("finalize container configuration");

    config.linux_->maskedPaths = maskedPaths;
    return LINGLONG_OK;
}

utils::error::Result<void> ContainerCfgBuilder::build() noexcept
{
    LINGLONG_TRACE("build container configuration");

    BUILD_STEP(checkValid);
    BUILD_STEP(prepare);
    BUILD_STEP(buildIdMappings);
    BUILD_STEP(buildMountRuntime);
    BUILD_STEP(buildMountApp);
    BUILD_STEP(buildXDGRuntime);
    BUILD_STEP(buildPrivateDir);
    BUILD_STEP(buildMountHome);
    BUILD_STEP(buildPrivateMapped);
    BUILD_STEP(buildMountIPC);
    BUILD_STEP(buildMountTimeZone);
    BUILD_STEP(buildMountNetworkConf);
    BUILD_STEP(buildDisplaySystem);
    BUILD_STEP(buildContainerInfo);
    BUILD_STEP(buildMountCache);
    BUILD_STEP(buildLDCache);
    BUILD_STEP(buildEnv);
    BUILD_STEP(buildHooks);
    BUILD_STEP(mergeMount);
    BUILD_STEP(finalize);
    BUILD_STEP(applyPatch);
    BUILD_STEP(selfAdjustingMount);

    return LINGLONG_OK;
}

utils::error::Result<void>
ContainerCfgBuilder::mountBind(const ocppi::runtime::config::types::Mount &mount) noexcept
{
    LINGLONG_TRACE(
      fmt::format("mount bind: {} -> {}", mount.source.value_or("[none]"), mount.destination));

    if (!mount.source) {
        return LINGLONG_ERR("mount source is not set");
    }

    unsigned long flags = MS_BIND | MS_REC;
    if (mount.options
        && std::find(mount.options->begin(), mount.options->end(), "ro") != mount.options->end()) {
        flags |= MS_RDONLY;
    }

    if (::mount(mount.source->c_str(), mount.destination.c_str(), "", flags, nullptr) != 0) {
        return LINGLONG_ERR(
          fmt::format("failed to mount {}: {}", mount.source.value(), strerror(errno)));
    }

    return LINGLONG_OK;
}

} // namespace linglong::generator
