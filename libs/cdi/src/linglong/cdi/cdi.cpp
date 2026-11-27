/*
 * SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "cdi.h"

#include "linglong/cdi/types/Generators.hpp"
#include "linglong/common/strings.h"
#include "linglong/utils/log/log.h"
#include "linglong/utils/sha256.h"
#include "ytj/ytj.hpp"

#include <fmt/ranges.h>
#include <nlohmann/json.hpp>
#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <set>
#include <sstream>

namespace linglong::cdi {

namespace {

bool isAsciiAlphanumeric(char ch)
{
    return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9');
}

bool validYamlKeys(const YAML::Node &node, unsigned int depth)
{
    if (depth == 0) {
        return false;
    }
    if (node.IsMap()) {
        std::set<std::string> keys;
        for (const auto &entry : node) {
            if (!entry.first.IsScalar()) {
                return false;
            }
            const auto key = ytj::to_json(entry.first);
            const auto *name = key.get_ptr<const std::string *>();
            if (name == nullptr || !keys.insert(*name).second
                || !validYamlKeys(entry.second, depth - 1)) {
                return false;
            }
        }
    } else if (node.IsSequence()) {
        for (const auto &entry : node) {
            if (!validYamlKeys(entry, depth - 1)) {
                return false;
            }
        }
    }
    return true;
}

utils::error::Result<types::Cdi> decodeYamlSpec(std::istream &input)
{
    LINGLONG_TRACE("parse CDI YAML");
    try {
        const auto yaml = YAML::Load(input);
        if (!yaml.IsMap()) {
            return LINGLONG_ERR("expected a CDI YAML mapping");
        }
        if (!validYamlKeys(yaml, 64)) {
            return LINGLONG_ERR("CDI YAML contains invalid keys or excessive nesting");
        }
        return ytj::to_json(yaml).get<types::Cdi>();
    } catch (const std::exception &error) {
        return LINGLONG_ERR(error);
    }
}

utils::error::Result<types::Cdi> decodeSpecDocument(std::istream &input)
{
    LINGLONG_TRACE("parse CDI specification");
    try {
        const auto document = nlohmann::json::parse(input, nullptr, false);
        if (!document.is_discarded()) {
            return document.get<types::Cdi>();
        }

        input.clear();
        input.seekg(0);
        return decodeYamlSpec(input);
    } catch (const std::exception &error) {
        return LINGLONG_ERR(error);
    }
}

utils::error::Result<types::Cdi> decodeCDISpec(std::istream &input)
{
    LINGLONG_TRACE("decode and validate CDI specification");
    auto spec = decodeSpecDocument(input);
    if (!spec) {
        return LINGLONG_ERR(spec);
    }
    auto validated = validateCDISpec(*spec);
    if (!validated) {
        return LINGLONG_ERR(validated);
    }
    return spec;
}

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

utils::error::Result<types::Cdi> parseCDISpecFile(const std::filesystem::path &specPath)
{
    LINGLONG_TRACE(fmt::format("parse CDI spec: {}", specPath.string()));

    std::ifstream input(specPath);
    if (!input.is_open()) {
        return LINGLONG_ERR(fmt::format("failed to open file: {}", specPath.string()));
    }

    auto spec = decodeCDISpec(input);
    if (!spec) {
        return LINGLONG_ERR(
          fmt::format("failed to parse CDI spec {}: {}", specPath.string(), spec.error().message()),
          spec);
    }
    return spec;
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

struct ParsedSpec
{
    std::filesystem::path path;
    std::string checksum;
    types::Cdi spec;
};

utils::error::Result<ParsedSpec> readSpec(const std::filesystem::path &path)
{
    LINGLONG_TRACE(fmt::format("read CDI spec {}", path.string()));

    std::error_code ec;
    const auto absolutePath = std::filesystem::absolute(path, ec);
    if (ec) {
        return LINGLONG_ERR("resolve CDI spec path", ec);
    }
    auto spec = parseCDISpecFile(absolutePath);
    if (!spec) {
        return LINGLONG_ERR(spec);
    }

    auto checksum = calculateSpecChecksum(absolutePath);
    if (!checksum) {
        return LINGLONG_ERR(checksum);
    }

    return ParsedSpec{ absolutePath, std::move(*checksum), std::move(*spec) };
}

api::types::v1::CdiDeviceEntry deviceEntry(const ParsedSpec &spec, const std::string &name)
{
    return { spec.spec.kind, name, { spec.checksum, spec.path.string() } };
}

void collectSpecDevices(std::vector<api::types::v1::CdiDeviceEntry> &entries,
                        const ParsedSpec &spec)
{
    for (const auto &device : spec.spec.devices) {
        entries.emplace_back(deviceEntry(spec, device.name));
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

bool validDeviceName(std::string_view name)
{
    if (name.empty() || !isAsciiAlphanumeric(name.front()) || !isAsciiAlphanumeric(name.back())) {
        return false;
    }
    return std::all_of(name.begin(), name.end(), [](char ch) {
        return isAsciiAlphanumeric(ch) || ch == '-' || ch == '_' || ch == '.' || ch == ':';
    });
}

bool validKind(std::string_view kind)
{
    const auto validSegment = [](std::string_view segment) {
        if (segment.empty() || !isAsciiAlphanumeric(segment.front())) {
            return false;
        }
        return std::all_of(segment.begin(), segment.end(), [](char ch) {
            return isAsciiAlphanumeric(ch) || ch == '-' || ch == '_' || ch == '.';
        });
    };
    const auto slash = kind.find('/');
    return slash != std::string_view::npos && validSegment(kind.substr(0, slash))
      && validSegment(kind.substr(slash + 1));
}

utils::error::Result<void> validateCDISpec(const types::Cdi &spec)
{
    LINGLONG_TRACE("validate CDI specification");
    if (!validKind(spec.kind)) {
        return LINGLONG_ERR("invalid CDI kind: " + spec.kind);
    }
    if (spec.cdiVersion.empty() || spec.devices.empty()) {
        return LINGLONG_ERR("CDI specification needs a version and at least one device");
    }
    std::set<std::string> names;
    for (const auto &device : spec.devices) {
        if (!validDeviceName(device.name) || !names.insert(device.name).second) {
            return LINGLONG_ERR("invalid or duplicate CDI device name: " + device.name);
        }
    }
    return LINGLONG_OK;
}

utils::error::Result<types::Cdi> parseCDISpec(const std::string &content)
{
    std::istringstream input(content);
    return decodeCDISpec(input);
}

utils::error::Result<std::vector<api::types::v1::CdiDeviceEntry>> getCDIDevices(
  const std::vector<std::string> &specDirs, const std::optional<std::vector<std::string>> &devices)
{
    std::string devicesStr;
    if (devices) {
        devicesStr = fmt::format("{}", fmt::join(*devices, ", "));
    } else {
        devicesStr = "(all)";
    }
    LINGLONG_TRACE(
      fmt::format("get CDI devices {} from {}", devicesStr, fmt::join(specDirs, ", ")));

    std::vector<ParsedSpec> specs;
    for (const auto &dir : specDirs) {
        std::error_code ec;
        if (!std::filesystem::is_directory(dir, ec)) {
            continue;
        }
        std::vector<std::filesystem::path> paths;
        for (const auto &entry : std::filesystem::directory_iterator(
               dir,
               std::filesystem::directory_options::skip_permission_denied,
               ec)) {
            const auto extension = entry.path().extension();
            if (extension != ".json" && extension != ".yaml" && extension != ".yml") {
                continue;
            }
            paths.emplace_back(entry.path());
        }
        // Preserve directory priority and make the order within each directory deterministic.
        std::sort(paths.begin(), paths.end());
        for (const auto &path : paths) {
            auto spec = readSpec(path);
            if (!spec) {
                LogW("skip unreadable CDI spec: {}", spec.error());
                continue;
            }
            specs.emplace_back(std::move(*spec));
        }
    }

    std::vector<api::types::v1::CdiDeviceEntry> result;

    if (!devices) {
        for (const auto &spec : specs) {
            collectSpecDevices(result, spec);
        }
        return result;
    }

    for (const auto &deviceStr : *devices) {
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
                        result.emplace_back(deviceEntry(spec, name));
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

utils::error::Result<std::vector<api::types::v1::CdiDeviceEntry>>
getCDIDevicesFromSpec(const std::filesystem::path &path)
{
    LINGLONG_TRACE(fmt::format("read CDI devices from {}", path.string()));
    auto spec = readSpec(path);
    if (!spec) {
        return LINGLONG_ERR(spec);
    }
    std::vector<api::types::v1::CdiDeviceEntry> result;
    collectSpecDevices(result, *spec);
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
