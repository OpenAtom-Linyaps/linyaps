/*
 * SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "run_context.h"

#include "linglong/extension/extension.h"
#include "linglong/utils/log/log.h"

#include <cstdlib>
#include <unordered_set>
#include <utility>

namespace linglong::runtime {

void RunContext::setupHostNvidiaFallbacks(
  generator::ContainerCfgBuilder &builder,
  std::vector<ocppi::runtime::config::types::Mount> &extensionMounts)
{
    if (!hostNvidiaFallbackEnabled) {
        return;
    }

    if (hostExtensions.empty() || !baseLayer) {
        return;
    }

    bool hasInstalledNvidiaExtension = false;
    for (const auto &ext : extensionLayers) {
        const auto &name = ext.getReference().id;
        if (extension::isNvidiaDisplayDriverExtension(name)) {
            hasInstalledNvidiaExtension = true;
            break;
        }
    }

    if (hasInstalledNvidiaExtension) {
        return;
    }

    auto hostBase = bundle / "host-extensions";
    auto appendPathEnv = [this](const std::string &key, const std::string &value) {
        if (value.empty()) {
            return;
        }
        auto it = environment.find(key);
        std::string current;
        if (it != environment.end() && !it->second.empty()) {
            current = it->second;
        } else {
            auto *envValue = ::getenv(key.c_str());
            if (envValue != nullptr && envValue[0] != '\0') {
                current = envValue;
            }
        }
        if (current.empty()) {
            environment[key] = value;
            return;
        }
        std::string haystack = ":" + current + ":";
        std::string needle = ":" + value + ":";
        if (haystack.find(needle) != std::string::npos) {
            environment[key] = current;
            return;
        }
        environment[key] = value + ":" + current;
    };
    auto setEnvIfEmpty = [this](const std::string &key, const std::string &value) {
        if (value.empty()) {
            return;
        }
        auto it = environment.find(key);
        if (it != environment.end() && !it->second.empty()) {
            return;
        }
        auto *envValue = ::getenv(key.c_str());
        if (envValue != nullptr && envValue[0] != '\0') {
            return;
        }
        environment[key] = value;
    };
    for (const auto &overrideDef : extensionOverrides) {
        if (extension::isNvidiaDisplayDriverExtension(overrideDef.name)) {
            return;
        }
    }

    std::unordered_set<std::string> deviceBindSet;
    auto addDeviceBind = [&](const std::filesystem::path &devicePath) {
        std::error_code ec;
        auto status = std::filesystem::status(devicePath, ec);
        if (ec) {
            return;
        }
        if (!std::filesystem::exists(status)) {
            return;
        }
        if (!std::filesystem::is_character_file(status)
            && !std::filesystem::is_block_file(status)
            && !std::filesystem::is_directory(status)) {
            return;
        }
        auto key = devicePath.string();
        if (!deviceBindSet.insert(key).second) {
            return;
        }
        builder.addExtraMount(ocppi::runtime::config::types::Mount{
          .destination = key,
          .options = { { "rbind" } },
          .source = key,
          .type = "bind",
        });
    };

    addDeviceBind("/dev/dxg");
    std::error_code devEc;
    for (const auto &entry : std::filesystem::directory_iterator("/dev", devEc)) {
        if (devEc) {
            break;
        }
        auto name = entry.path().filename().string();
        if (name.rfind("nvidia", 0) != 0) {
            continue;
        }
        addDeviceBind(entry.path());
    }

    for (const auto &name : hostExtensions) {
        auto hostExt = extension::prepareHostNvidiaExtension(hostBase, name);
        if (!hostExt) {
            LogW("failed to prepare host NVIDIA driver for {}", name);
            continue;
        }
        extensionMounts.push_back(ocppi::runtime::config::types::Mount{
          .destination = "/opt/extensions/" + name,
          .gidMappings = {},
          .options = { { "rbind", "ro" } },
          .source = hostExt->root.string(),
          .type = "bind",
          .uidMappings = {},
        });

        for (const auto &bind : hostExt->extraBinds) {
            ocppi::runtime::config::types::Mount mount = {
                .destination = bind.destination.string(),
                .options = { { "rbind", "ro" } },
                .source = bind.source.string(),
                .type = "bind",
            };
            builder.addExtraMount(mount);
        }

        {
            std::error_code ec;
            auto extensionRoot = std::filesystem::path("/opt/extensions") / name;
            auto libPath = extensionRoot / "orig";
            auto lib32Path = extensionRoot / "orig/32";
            if (std::filesystem::exists(hostExt->root / "orig/32", ec)) {
                appendPathEnv("LD_LIBRARY_PATH", lib32Path.string());
            }
            ec.clear();
            if (std::filesystem::exists(hostExt->root / "orig", ec)) {
                appendPathEnv("LD_LIBRARY_PATH", libPath.string());
                setEnvIfEmpty("NVIDIA_CTK_LIBCUDA_DIR", libPath.string());
            }
        }

        if (hostExt->vkIcdFile) {
            appendPathEnv("VK_ADD_DRIVER_FILES", hostExt->vkIcdFile->string());
            appendPathEnv("VK_ICD_FILENAMES", hostExt->vkIcdFile->string());
        }
        if (hostExt->eglExternalPlatformDir) {
            appendPathEnv("__EGL_EXTERNAL_PLATFORM_CONFIG_DIRS",
                          hostExt->eglExternalPlatformDir->string());
            appendPathEnv("EGL_EXTERNAL_PLATFORM_CONFIG_DIRS",
                          hostExt->eglExternalPlatformDir->string());
        }
        if (hostExt->eglVendorDir) {
            appendPathEnv("__EGL_VENDOR_LIBRARY_DIRS", hostExt->eglVendorDir->string());
        }
        if (hostExt->hasGlxLib) {
            setEnvIfEmpty("__GLX_VENDOR_LIBRARY_NAME", "nvidia");
            setEnvIfEmpty("__NV_PRIME_RENDER_OFFLOAD", "1");
        }
    }
}

} // namespace linglong::runtime
