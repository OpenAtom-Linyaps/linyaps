/*
 * SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "cdi.h"

#include "linglong/cdi/types/Cdi.hpp"
#include "linglong/cdi/types/Generators.hpp"
#include "linglong/common/strings.h"
#include "linglong/utils/log/log.h"
#include "linglong/utils/sha256.h"
#include "ytj/ytj.hpp"

#include <fmt/ranges.h>
#include <nlohmann/json.hpp>
#include <yaml-cpp/yaml.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace linglong::cdi {

namespace {

template <typename T>
void mergeVector(std::optional<std::vector<T>> &target, const std::optional<std::vector<T>> &source)
{
    if (!source) {
        return;
    }
    if (!target) {
        target = source;
        return;
    }
    target->insert(target->end(), source->begin(), source->end());
}

types::ContainerEdits mergeEdits(const types::ContainerEdits &global,
                                 const types::ContainerEdits &local)
{
    types::ContainerEdits result = global;

    mergeVector(result.additionalGids, local.additionalGids);
    mergeVector(result.deviceNodes, local.deviceNodes);
    mergeVector(result.env, local.env);
    mergeVector(result.hooks, local.hooks);
    mergeVector(result.mounts, local.mounts);
    mergeVector(result.netDevices, local.netDevices);

    if (local.intelRdt) {
        if (!result.intelRdt) {
            result.intelRdt = local.intelRdt;
        } else {
            auto &r = result.intelRdt.value();
            const auto &l = local.intelRdt.value();
            if (l.closID) {
                r.closID = l.closID;
            }
            if (l.enableMonitoring) {
                r.enableMonitoring = l.enableMonitoring;
            }
            if (l.l3CacheSchema) {
                r.l3CacheSchema = l.l3CacheSchema;
            }
            if (l.memBWSchema) {
                r.memBWSchema = l.memBWSchema;
            }
            mergeVector(r.schemata, l.schemata);
        }
    }

    return result;
}

utils::error::Result<types::Cdi> parseCDISpecFile(std::filesystem::path specPath)
{
    LINGLONG_TRACE(fmt::format("parse CDI spec: {}", specPath.string()));

    try {
        std::ifstream i(specPath);
        if (!i.is_open()) {
            return LINGLONG_ERR(fmt::format("failed to open file: {}", specPath.string()));
        }

        nlohmann::json j;
        if (specPath.extension() == ".yaml" || specPath.extension() == ".yml") {
            YAML::Node node = YAML::Load(i);
            j = ytj::to_json(node);
        } else if (specPath.extension() == ".json") {
            i >> j;
        } else {
            return LINGLONG_ERR(fmt::format("invalid file extension: {}", specPath.string()));
        }

        return j.get<types::Cdi>();
    } catch (const std::exception &e) {
        return LINGLONG_ERR(
          fmt::format("failed to parse CDI spec {}: {}", specPath.string(), e.what()));
    }
}

utils::error::Result<std::string> calculateSpecChecksum(const std::filesystem::path &specPath)
{
    LINGLONG_TRACE(fmt::format("calculate CDI spec checksum {}", specPath.string()));

    try {
        std::ifstream file(specPath, std::ios::binary);
        if (!file.is_open()) {
            return LINGLONG_ERR(fmt::format("failed to open file: {}", specPath.string()));
        }

        digest::SHA256 hash;
        std::array<std::byte, 8192> buffer{};
        while (file) {
            file.read(reinterpret_cast<char *>(buffer.data()), buffer.size());
            auto readBytes = file.gcount();
            if (readBytes > 0) {
                hash.update(buffer.data(), static_cast<std::size_t>(readBytes));
            }
        }

        if (file.bad()) {
            return LINGLONG_ERR(fmt::format("failed to read file: {}", specPath.string()));
        }

        std::array<std::byte, 32> digest{};
        hash.final(digest.data());

        std::stringstream stream;
        stream << std::setfill('0') << std::hex;
        for (auto byte : digest) {
            stream << std::setw(2) << static_cast<unsigned int>(byte);
        }

        return stream.str();
    } catch (const std::exception &e) {
        return LINGLONG_ERR(
          fmt::format("failed to calculate CDI spec checksum {}: {}", specPath.string(), e.what()));
    }
}

utils::error::Result<types::ContainerEdits> getCDIDeviceEdits(const types::Cdi &spec,
                                                              std::string_view deviceName)
{
    LINGLONG_TRACE(fmt::format("get CDI device edits {}={}", spec.kind, deviceName));

    for (const auto &dev : spec.devices) {
        if (dev.name != deviceName) {
            continue;
        }

        types::ContainerEdits globalEdits;
        if (spec.containerEdits) {
            globalEdits = spec.containerEdits.value();
        }
        return mergeEdits(globalEdits, dev.containerEdits);
    }

    return LINGLONG_ERR(fmt::format("device not found: {}={}", spec.kind, deviceName));
}

} // namespace

utils::error::Result<std::vector<api::types::v1::CdiDeviceEntry>>
getCDIDevices(const std::vector<std::string> &specDirs, const std::vector<std::string> &devices)
{
    LINGLONG_TRACE(fmt::format("get CDI devices {} from {}",
                               fmt::join(devices, ", "),
                               fmt::join(specDirs, ", ")));

    struct ParsedSpec
    {
        std::filesystem::path path;
        std::string checksum;
        types::Cdi spec;
    };

    std::vector<ParsedSpec> specs;
    for (const auto &dir : specDirs) {
        std::error_code ec;
        if (!std::filesystem::is_directory(dir, ec)) {
            continue;
        }
        for (const auto &entry : std::filesystem::directory_iterator(
               dir,
               std::filesystem::directory_options::skip_permission_denied,
               ec)) {
            auto res = parseCDISpecFile(entry.path());
            if (!res) {
                LogW("skip error parsed CDI spec: {}", res.error());
                continue;
            }

            auto checksum = calculateSpecChecksum(entry.path());
            if (!checksum) {
                LogW("skip CDI spec with invalid checksum: {}", checksum.error());
                continue;
            }

            specs.emplace_back(ParsedSpec{
              .path = entry.path(),
              .checksum = std::move(*checksum),
              .spec = std::move(*res),
            });
        }
    }

    std::vector<api::types::v1::CdiDeviceEntry> result;
    for (const auto &deviceStr : devices) {
        auto device = common::strings::split(deviceStr, '=');
        if (device.size() != 2) {
            return LINGLONG_ERR(fmt::format("invalid device format: {}", deviceStr));
        }
        std::string kind{ device[0] };
        std::string name{ device[1] };

        bool found = false;
        for (const auto &spec : specs) {
            if (spec.spec.kind == kind) {
                for (const auto &dev : spec.spec.devices) {
                    if (dev.name == name) {
                        result.emplace_back(api::types::v1::CdiDeviceEntry{
                          .kind = kind,
                          .name = name,
                          .spec =
                            api::types::v1::CdiSpec{
                              .checksum = spec.checksum,
                              .path = spec.path.string(),
                            },
                        });
                        found = true;
                        break;
                    }
                }
            }
            if (found) {
                break;
            }
        }
        if (!found) {
            return LINGLONG_ERR(fmt::format("device not found: {}", deviceStr));
        }
    }
    return result;
}

utils::error::Result<types::ContainerEdits>
getCDIDeviceEdits(const api::types::v1::CdiDeviceEntry &device)
{
    LINGLONG_TRACE(fmt::format("get CDI device edits from entry {}={}", device.kind, device.name));

    auto spec = parseCDISpecFile(device.spec.path);
    if (!spec) {
        return LINGLONG_ERR(spec);
    }

    auto checksum = calculateSpecChecksum(device.spec.path);
    if (!checksum) {
        return LINGLONG_ERR(checksum);
    }

    if (!device.spec.checksum.empty() && *checksum != device.spec.checksum) {
        return LINGLONG_ERR(fmt::format("CDI spec checksum mismatch: {}", device.spec.path));
    }

    if (spec->kind != device.kind) {
        return LINGLONG_ERR(
          fmt::format("CDI device kind mismatch: expected {}, got {}", device.kind, spec->kind));
    }

    return getCDIDeviceEdits(*spec, device.name);
}

} // namespace linglong::cdi
