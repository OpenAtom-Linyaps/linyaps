// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linglong/runtime/cdi_config.h"

#include "linglong/cdi/cdi.h"
#include "linglong/cli/cli.h"
#include "linglong/common/strings.h"
#include "linglong/utils/log/log.h"
#include "linglong/utils/runtime_config.h"

#include <algorithm>
#include <map>
#include <set>
#include <utility>

namespace linglong::runtime {

namespace {

using CdiEntries = std::vector<api::types::v1::CdiDeviceEntry>;
using api::types::v1::CdiDeviceRequest;
using api::types::v1::Device;
using DeviceRequests = std::vector<Device>;
using api::types::v1::RuntimeConfigure;

const std::string &qualifiedName(const Device &device)
{
    const auto *bound = std::get_if<CdiDeviceRequest>(&device);
    return bound ? bound->name : std::get<std::string>(device);
}

std::optional<DeviceRequests> configuredDevices(const DeviceRequests &generated,
                                                const std::optional<RuntimeConfigure> &configured)
{
    if (!configured || !configured->devices) {
        if (generated.empty()) {
            return std::nullopt;
        }
        return generated;
    }
    const auto &devices = *configured->devices;
    if (devices.empty() || generated.empty()) {
        return devices;
    }

    std::set<std::string> configuredNames;
    for (const auto &device : devices) {
        configuredNames.insert(qualifiedName(device));
    }
    DeviceRequests result;
    result.reserve(generated.size() + devices.size());
    for (const auto &device : generated) {
        if (configuredNames.count(qualifiedName(device)) == 0) {
            result.push_back(device);
        }
    }
    result.insert(result.end(), devices.begin(), devices.end());
    return result;
}

utils::error::Result<CdiEntries> resolveDeviceRequests(const std::vector<std::string> &specDirs,
                                                       const DeviceRequests &requests)
{
    LINGLONG_TRACE("resolve CDI devices");

    CdiEntries result;
    std::optional<CdiEntries> scanned;
    std::map<std::string, CdiEntries> specs;
    std::map<std::string, std::string> selected;

    for (const auto &request : requests) {
        const auto *bound = std::get_if<CdiDeviceRequest>(&request);
        const auto &name = qualifiedName(request);
        const auto parts = common::strings::split(name, '=');
        if (parts.size() != 2 || parts[0].empty() || parts[1].empty()) {
            return LINGLONG_ERR(fmt::format("invalid device format: {}", name));
        }
        if (const auto previous = selected.find(name); previous != selected.end()) {
            LogW("Skipping repeated CDI device {} from {}; keeping {}",
                 name,
                 bound ? bound->spec : "<spec search>",
                 previous->second);
            continue;
        }

        const CdiEntries *available = nullptr;
        if (bound) {
            auto spec = specs.find(bound->spec);
            if (spec == specs.end()) {
                // The run context can be restored in a process with a different working directory.
                std::error_code ec;
                const auto path = std::filesystem::absolute(bound->spec, ec);
                if (ec) {
                    return LINGLONG_ERR(fmt::format("resolve CDI spec {}", bound->spec), ec);
                }
                auto devices = cdi::getCDIDevicesFromSpec(path);
                if (!devices) {
                    return LINGLONG_ERR(fmt::format("read CDI spec {}", bound->spec), devices);
                }
                spec = specs.emplace(bound->spec, std::move(*devices)).first;
            }
            available = &spec->second;
        } else {
            if (!scanned) {
                auto devices = cdi::getCDIDevices(specDirs, std::nullopt);
                if (!devices) {
                    return LINGLONG_ERR(devices);
                }
                scanned = std::move(*devices);
            }
            available = &*scanned;
        }

        const auto matches = [&parts](const api::types::v1::CdiDeviceEntry &entry) {
            return entry.kind == parts[0] && entry.name == parts[1];
        };
        auto entry = std::find_if(available->begin(), available->end(), matches);
        if (entry == available->end()) {
            if (bound) {
                return LINGLONG_ERR(fmt::format("device not found: {} in {}", name, bound->spec));
            }
            return LINGLONG_ERR(fmt::format("device not found: {}", name));
        }
        selected.emplace(name, entry->spec.path);
        LogD("Selected CDI device {} from {}", name, entry->spec.path);
        result.emplace_back(*entry);
    }

    return result;
}

} // namespace

CdiDriverGeneration loadCdiDriverGeneration(const std::filesystem::path &runtimeDir)
{
    CdiDriverGeneration published;
    const auto current = runtimeDir / "current";
    std::error_code ec;
    const auto generation = std::filesystem::canonical(current, ec);
    if (ec) {
        if (ec != std::errc::no_such_file_or_directory) {
            LogW("Skipping CDI driver generation {}: {}", current, ec.message());
        }
        return published;
    }

    // Pin one generation for both the generated config and its CDI specs.
    published.specDir = generation / "spec";
    const auto directory = generation / "cfg";
    auto devices = utils::loadCdiDeviceDropIns(directory);
    if (!devices) {
        LogW("Skipping generated CDI config in {}: {}", directory, devices.error());
    } else {
        published.devices = std::move(*devices);
    }
    return published;
}

utils::error::Result<std::optional<CdiEntries>>
resolveCdiDevices(const CdiDriverGeneration &generation,
                  const std::optional<RuntimeConfigure> &configured,
                  const cli::RunOptions &options)
{
    LINGLONG_TRACE("resolve CDI config and CLI requests");
    const auto requests = options.cdiDevices.empty()
      ? configuredDevices(generation.devices, configured)
      : std::optional<DeviceRequests>{ options.cdiDevices };

    if (requests) {
        auto specDirs = options.cdiSpecDir;
        if (!generation.specDir.empty()) {
            specDirs.insert(specDirs.begin(), generation.specDir.string());
        }
        auto devices = resolveDeviceRequests(specDirs, *requests);
        if (!devices) {
            return LINGLONG_ERR(devices);
        }
        return std::optional<CdiEntries>{ std::move(*devices) };
    }

    auto devices = cdi::getCDIDevices(options.cdiSpecDir, std::nullopt);
    if (!devices) {
        return LINGLONG_ERR(devices);
    }
    const auto nvidia = std::find_if(devices->begin(), devices->end(), [](const auto &device) {
        return device.kind == "nvidia.com/gpu" && device.name == "all";
    });
    if (nvidia != devices->end()) {
        LogD("{}={} detected", nvidia->kind, nvidia->name);
        return std::optional<CdiEntries>{ CdiEntries{ *nvidia } };
    }
    return std::nullopt;
}

} // namespace linglong::runtime
