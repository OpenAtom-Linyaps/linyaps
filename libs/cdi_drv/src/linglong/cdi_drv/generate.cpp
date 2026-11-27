// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "generate.h"

#include "drm.h"
#include "linglong/api/types/v1/Generators.hpp"
#include "linglong/cdi/cdi.h"
#include "linglong/cdi/types/Generators.hpp"
#include "linglong/cdi_drv/types/Generators.hpp"
#include "linglong/utils/cmd.h"
#include "linglong/utils/log/log.h"

#include <fmt/format.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <set>
#include <string_view>
#include <system_error>
#include <utility>

namespace linglong::cdi_drv {
namespace {

namespace fs = std::filesystem;
namespace cdiTypes = linglong::cdi::types;
using nlohmann::json;
using utils::error::Result;

std::string deviceConfig(const std::string &specFilename,
                         const types::CdiDrvConfig &config,
                         const cdiTypes::Cdi &spec,
                         const std::string &selected)
{
    std::vector<api::types::v1::CdiDeviceRequest> devices;
    if (config.autoEnable.value_or(true)) {
        const auto found =
          std::any_of(spec.devices.begin(), spec.devices.end(), [&](const auto &device) {
              return device.name == selected;
          });
        if (found) {
            devices.push_back({ spec.kind + "=" + selected, "../spec/" + specFilename });
        } else {
            LogW("CDI default device {}={} is unavailable", spec.kind, selected);
        }
    }
    return json{ { "devices", devices } }.dump();
}

std::string resolveMountSource(std::string_view path)
{
    const auto *environmentLibDir = std::getenv("LINGLONG_CDI_DRV_LIB_DIR");
    const std::string_view libDir =
      environmentLibDir && *environmentLibDir ? environmentLibDir : LINGLONG_CDI_DRV_LIB_DIR;
    constexpr std::string_view variable = "${LIB_DIR}";

    std::string result;
    result.reserve(path.size());
    for (std::size_t pos = 0; pos < path.size();) {
        if (path.substr(pos, 2) == "$$") {
            result.push_back('$');
            pos += 2;
        } else if (path.substr(pos, variable.size()) == variable) {
            result.append(libDir);
            pos += variable.size();
        } else {
            result.push_back(path[pos++]);
        }
    }
    return result;
}

struct MergedProfile
{
    std::vector<types::DeviceNode> deviceNodes;
    std::vector<types::MountFileElement> mountFiles;
    std::vector<std::string> env;
    std::optional<types::DrmSelector> drm;
};

MergedProfile mergeProfiles(const types::Profile &global, const types::Profile &profile)
{
    LINGLONG_TRACE("merge CDI profile fields");

    MergedProfile merged;
    for (const auto *source : { &global, &profile }) {
        if (source->deviceNodes) {
            merged.deviceNodes.insert(merged.deviceNodes.end(),
                                      source->deviceNodes->begin(),
                                      source->deviceNodes->end());
        }
        if (source->mountFiles) {
            merged.mountFiles.insert(merged.mountFiles.end(),
                                     source->mountFiles->begin(),
                                     source->mountFiles->end());
        }
        if (source->env) {
            merged.env.insert(merged.env.end(), source->env->begin(), source->env->end());
        }
        if (source->drm) {
            merged.drm = source->drm;
        }
    }
    return merged;
}

Result<std::optional<cdiTypes::Device>> makeDevice(const std::string &name, MergedProfile profile)
{
    LINGLONG_TRACE("generate CDI device " + name);

    if (profile.drm) {
        auto nodes = drmNodes(*profile.drm);
        if (!nodes) {
            return LINGLONG_ERR(nodes);
        }
        for (const auto &node : *nodes) {
            profile.deviceNodes.push_back({ std::nullopt, node, true });
        }
    }

    std::vector<cdiTypes::DeviceNode> devices;
    for (const auto &device : profile.deviceNodes) {
        auto target = device.path;
        auto source = device.hostPath.value_or(device.path);

        std::error_code ec;
        const auto status = fs::status(source, ec);
        if (ec && ec != std::errc::no_such_file_or_directory) {
            return LINGLONG_ERR(fmt::format("check CDI source {}", source), ec);
        }
        if (!fs::exists(status)) {
            if (device.required.value_or(true)) {
                return LINGLONG_ERR("required path is missing: " + source);
            }
            continue;
        }

        cdiTypes::DeviceNode node{};
        node.path = std::move(target);
        if (node.path != source) {
            node.hostPath = std::move(source);
        }
        devices.push_back(std::move(node));
    }

    std::vector<cdiTypes::Mount> mounts;
    for (const auto &entry : profile.mountFiles) {
        const auto *file = std::get_if<types::MountFile>(&entry);
        auto source = resolveMountSource(file ? file->hostPath : std::get<std::string>(entry));
        auto target = file ? file->path.value_or(source) : source;

        std::error_code ec;
        const auto status = fs::status(source, ec);
        if (ec && ec != std::errc::no_such_file_or_directory) {
            return LINGLONG_ERR(fmt::format("check CDI source {}", source), ec);
        }
        if (!fs::exists(status)) {
            if (!file || file->required.value_or(true)) {
                return LINGLONG_ERR("required path is missing: " + source);
            }
            continue;
        }

        mounts.push_back({ std::move(target),
                           std::move(source),
                           std::vector<std::string>{ "ro", "nosuid", "nodev", "rbind", "rprivate" },
                           "bind" });
    }

    auto &env = profile.env;
    if (devices.empty() && mounts.empty() && env.empty()) {
        return LINGLONG_OK;
    }

    cdiTypes::ContainerEdits edits{};
    if (!devices.empty()) {
        edits.deviceNodes = std::move(devices);
    }
    if (!mounts.empty()) {
        edits.mounts = std::move(mounts);
    }
    if (!env.empty()) {
        edits.env = std::move(env);
    }
    return std::optional{ cdiTypes::Device{ std::nullopt, std::move(edits), name } };
}

Result<std::optional<GeneratedFiles>> generateBuiltin(const std::string &name,
                                                      const types::CdiDrvConfig &config)
{
    LINGLONG_TRACE("run builtin CDI generator");

    const bool hasProfiles = config.deviceProfile && !config.deviceProfile->empty();
    if (!config.kind || (!config.profile && !hasProfiles)) {
        return LINGLONG_ERR("linglong generator requires a kind and a profile");
    }
    if (!cdi::validKind(*config.kind)) {
        return LINGLONG_ERR("invalid CDI driver kind: " + *config.kind);
    }

    if (hasProfiles && config.defaultDevice) {
        const auto &selected = *config.defaultDevice;
        const bool found = std::any_of(config.deviceProfile->begin(),
                                       config.deviceProfile->end(),
                                       [&](const auto &profile) {
                                           return profile.device == selected;
                                       });
        if (!found) {
            return LINGLONG_ERR("defaultDevice does not name a configured device");
        }
    }

    auto profiles = config.deviceProfile.value_or(std::vector<types::DeviceProfile>{});
    if (profiles.empty() && config.profile) {
        profiles.push_back({ config.defaultDevice.value_or(name + "-linglong"), {} });
    }

    std::set<std::string> names;
    for (const auto &profile : profiles) {
        if (!cdi::validDeviceName(profile.device)) {
            return LINGLONG_ERR("invalid CDI driver device name: " + profile.device);
        }
        if (!names.insert(profile.device).second) {
            return LINGLONG_ERR("duplicate CDI driver device name: " + profile.device);
        }
    }

    const auto global = config.profile.value_or(types::Profile{});
    cdiTypes::Cdi spec{ std::nullopt, "0.6.0", std::nullopt, {}, *config.kind };
    for (const auto &profile : profiles) {
        auto merged = mergeProfiles(global, profile.profile);
        auto device = makeDevice(profile.device, std::move(merged));
        if (!device) {
            LogE("Skipping CDI device {}: {}", profile.device, device.error().message());
        } else if (*device) {
            spec.devices.push_back(std::move(**device));
        }
    }
    if (spec.devices.empty()) {
        return LINGLONG_OK;
    }

    const auto selected = config.defaultDevice.value_or(spec.devices.front().name);
    const auto specFilename = name + ".json";
    return std::optional{ GeneratedFiles{ specFilename,
                                          json(spec).dump(),
                                          deviceConfig(specFilename, config, spec, selected) } };
}

} // namespace

Result<std::optional<GeneratedFiles>> generate(const std::string &name,
                                               const types::CdiDrvConfig &config)
{
    LINGLONG_TRACE("generate CDI files for " + name);

    if (config.generator == types::Generator::Linglong) {
        return generateBuiltin(name, config);
    }

    if (!config.argv || config.argv->empty() || config.argv->front().empty()) {
        return LINGLONG_ERR("command generator requires a non-empty executable in argv[0]");
    }
    const auto timeout = std::max(config.timeout.value_or(30), std::int64_t{ 0 });
    if (timeout > std::chrono::duration_cast<std::chrono::seconds>(std::chrono::milliseconds::max())
                    .count()) {
        return LINGLONG_ERR("command generator timeout is too large");
    }
    const auto &argv = *config.argv;
    const std::vector<std::string> args(argv.begin() + 1, argv.end());
    auto output = utils::Cmd(argv.front()).setTimeout(std::chrono::seconds(timeout)).exec(args);
    if (!output) {
        return LINGLONG_ERR(output);
    }

    auto spec = cdi::parseCDISpec(*output);
    if (!spec) {
        return LINGLONG_ERR(spec);
    }
    const auto selected = config.defaultDevice.value_or(spec->devices.front().name);
    const auto specFilename = name + (json::accept(*output) ? ".json" : ".yaml");
    return std::optional{ GeneratedFiles{ specFilename,
                                          std::move(*output),
                                          deviceConfig(specFilename, config, *spec, selected) } };
}

} // namespace linglong::cdi_drv
