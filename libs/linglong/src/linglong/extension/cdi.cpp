/*
 * SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/extension/cdi.h"

#include "linglong/utils/command/cmd.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <unordered_set>

namespace linglong::extension::cdi {

namespace {

using linglong::utils::command::Cmd;

utils::error::Result<nlohmann::json> loadJsonFile(const std::filesystem::path &path)
{
    LINGLONG_TRACE("load CDI json file");

    std::ifstream file(path);
    if (!file.is_open()) {
        return LINGLONG_ERR("failed to open file: " + path.string());
    }

    nlohmann::json root;
    try {
        file >> root;
    } catch (const std::exception &ex) {
        return LINGLONG_ERR(std::string("failed to parse json: ") + ex.what());
    }

    return root;
}

utils::error::Result<nlohmann::json> loadJsonString(const std::string &text)
{
    LINGLONG_TRACE("parse CDI json string");

    nlohmann::json root;
    try {
        root = nlohmann::json::parse(text);
    } catch (const std::exception &ex) {
        auto extractPayload = [&](char open, char close) -> std::optional<std::string> {
            auto start = text.find(open);
            if (start == std::string::npos) {
                return std::nullopt;
            }

            int depth = 0;
            bool inString = false;
            bool escaped = false;
            for (size_t i = start; i < text.size(); ++i) {
                char c = text[i];
                if (inString) {
                    if (escaped) {
                        escaped = false;
                    } else if (c == '\\') {
                        escaped = true;
                    } else if (c == '"') {
                        inString = false;
                    }
                    continue;
                }
                if (c == '"') {
                    inString = true;
                    continue;
                }
                if (c == open) {
                    ++depth;
                } else if (c == close) {
                    --depth;
                    if (depth == 0) {
                        return text.substr(start, i - start + 1);
                    }
                }
            }
            return std::nullopt;
        };

        std::optional<std::string> payload = extractPayload('{', '}');
        if (!payload) {
            payload = extractPayload('[', ']');
        }
        if (!payload) {
            return LINGLONG_ERR(std::string("failed to parse json: ") + ex.what()
                                + "; no JSON payload found");
        }

        try {
            root = nlohmann::json::parse(*payload);
        } catch (const std::exception &inner) {
            return LINGLONG_ERR(std::string("failed to parse json: ") + ex.what()
                                + "; fallback parse failed: " + inner.what());
        }
    }
    return root;
}

void mergeEnvFromJson(const nlohmann::json &envJson, std::map<std::string, std::string> &env)
{
    if (envJson.is_object()) {
        for (auto it = envJson.begin(); it != envJson.end(); ++it) {
            if (it.value().is_string()) {
                env[it.key()] = it.value().get<std::string>();
            }
        }
        return;
    }

    if (!envJson.is_array()) {
        return;
    }

    for (const auto &item : envJson) {
        if (!item.is_string()) {
            continue;
        }
        const auto entry = item.get<std::string>();
        auto pos = entry.find('=');
        if (pos == std::string::npos || pos == 0) {
            continue;
        }
        env[entry.substr(0, pos)] = entry.substr(pos + 1);
    }
}

void mergeMountsFromJson(const nlohmann::json &mountsJson,
                         std::vector<ocppi::runtime::config::types::Mount> &mounts,
                         std::unordered_set<std::string> &seen)
{
    if (!mountsJson.is_array()) {
        return;
    }

    for (const auto &item : mountsJson) {
        if (!item.is_object()) {
            continue;
        }
        std::string source = item.value("hostPath", "");
        if (source.empty()) {
            source = item.value("source", "");
        }
        std::string destination = item.value("containerPath", "");
        if (destination.empty()) {
            destination = item.value("destination", "");
        }
        if (source.empty() || destination.empty()) {
            continue;
        }

        std::string key = destination + "|" + source;
        if (!seen.insert(key).second) {
            continue;
        }

        std::vector<std::string> options;
        auto optionsJson = item.find("options");
        if (optionsJson != item.end()) {
            if (optionsJson->is_array()) {
                for (const auto &opt : *optionsJson) {
                    if (opt.is_string()) {
                        options.emplace_back(opt.get<std::string>());
                    }
                }
            } else if (optionsJson->is_string()) {
                options.emplace_back(optionsJson->get<std::string>());
            }
        }
        if (options.empty()) {
            options = { "rbind", "ro" };
        }

        std::string type = item.value("type", "bind");
        mounts.push_back(ocppi::runtime::config::types::Mount{
          .destination = destination,
          .options = options,
          .source = source,
          .type = type,
        });
    }
}

void mergeDeviceNodesFromJson(const nlohmann::json &nodesJson,
                              std::vector<api::types::v1::DeviceNode> &nodes,
                              std::unordered_set<std::string> &seen)
{
    if (!nodesJson.is_array()) {
        return;
    }

    for (const auto &item : nodesJson) {
        if (!item.is_object()) {
            continue;
        }
        std::string path = item.value("path", "");
        if (path.empty()) {
            continue;
        }
        if (!seen.insert(path).second) {
            continue;
        }

        std::optional<std::string> hostPath;
        auto hostPathValue = item.find("hostPath");
        if (hostPathValue != item.end() && hostPathValue->is_string()) {
            hostPath = hostPathValue->get<std::string>();
        } else {
            auto hostPathAlt = item.find("host_path");
            if (hostPathAlt != item.end() && hostPathAlt->is_string()) {
                hostPath = hostPathAlt->get<std::string>();
            }
        }

        nodes.emplace_back(api::types::v1::DeviceNode{
          .hostPath = hostPath,
          .path = path,
        });
    }
}

void mergeContainerEdits(const nlohmann::json &editsJson,
                         ContainerEdits &edits,
                         std::unordered_set<std::string> &mountKeys,
                         std::unordered_set<std::string> &nodeKeys)
{
    if (!editsJson.is_object()) {
        return;
    }

    auto env = editsJson.find("env");
    if (env != editsJson.end()) {
        mergeEnvFromJson(*env, edits.env);
    }

    auto mounts = editsJson.find("mounts");
    if (mounts != editsJson.end()) {
        mergeMountsFromJson(*mounts, edits.mounts, mountKeys);
    }

    auto deviceNodes = editsJson.find("deviceNodes");
    if (deviceNodes != editsJson.end()) {
        mergeDeviceNodesFromJson(*deviceNodes, edits.deviceNodes, nodeKeys);
    }
}

utils::error::Result<ContainerEdits> parseContainerEdits(const nlohmann::json &cdi)
{
    LINGLONG_TRACE("parse CDI container edits");

    ContainerEdits edits;
    std::unordered_set<std::string> mountKeys;
    std::unordered_set<std::string> nodeKeys;

    auto mergeSpec = [&](const nlohmann::json &spec) {
        if (!spec.is_object()) {
            return;
        }
        auto containerEdits = spec.find("containerEdits");
        if (containerEdits != spec.end()) {
            mergeContainerEdits(*containerEdits, edits, mountKeys, nodeKeys);
        }
        auto devices = spec.find("devices");
        if (devices != spec.end() && devices->is_array()) {
            for (const auto &device : *devices) {
                if (!device.is_object()) {
                    continue;
                }
                auto deviceEdits = device.find("containerEdits");
                if (deviceEdits != device.end()) {
                    mergeContainerEdits(*deviceEdits, edits, mountKeys, nodeKeys);
                }
            }
        }
    };

    if (cdi.is_array()) {
        for (const auto &spec : cdi) {
            mergeSpec(spec);
        }
    } else {
        mergeSpec(cdi);
    }

    if (edits.empty()) {
        return LINGLONG_ERR("no containerEdits found in CDI spec; "
                            "please provide a valid CDI JSON");
    }

    return edits;
}

} // namespace

utils::error::Result<ContainerEdits> loadFromFile(const std::filesystem::path &path) noexcept
{
    LINGLONG_TRACE("load CDI from file");

    auto root = loadJsonFile(path);
    if (!root) {
        return LINGLONG_ERR(root.error());
    }

    return parseContainerEdits(*root);
}

utils::error::Result<ContainerEdits> loadFromJson(const std::string &jsonText) noexcept
{
    LINGLONG_TRACE("load CDI from json string");

    auto root = loadJsonString(jsonText);
    if (!root) {
        return LINGLONG_ERR(root.error());
    }

    return parseContainerEdits(*root);
}

utils::error::Result<ContainerEdits> loadFromNvidiaCtk() noexcept
{
    LINGLONG_TRACE("load CDI from nvidia-ctk");

    Cmd cmd("nvidia-ctk");
    if (!cmd.exists()) {
        return LINGLONG_ERR("nvidia-ctk not found");
    }

    auto output = cmd.exec({ "cdi", "generate", "--format", "json" });
    if (!output) {
        return LINGLONG_ERR(output.error());
    }

    return loadFromJson(output->toStdString());
}

utils::error::Result<ContainerEdits> loadFromHostNvidia() noexcept
{
    LINGLONG_TRACE("load CDI from host NVIDIA (builtin)");

    ContainerEdits edits;

    auto addMountIfExists = [&](const std::filesystem::path &path,
                                const std::vector<std::string> &options,
                                bool noexec = false) {
        if (path.empty()) {
            return;
        }
        std::error_code ec;
        if (!std::filesystem::exists(path, ec) || ec) {
            return;
        }
        std::vector<std::string> opts = options;
        if (noexec) {
            opts.push_back("noexec");
        }
        edits.mounts.push_back(ocppi::runtime::config::types::Mount{
          .destination = path.string(),
          .options = opts,
          .source = path.string(),
          .type = "bind",
        });
    };

    // Options modeled after nvidia-ctk generated CDI mounts.
    const std::vector<std::string> kRoDirOptions = {
        "ro",
        "nosuid",
        "nodev",
        "rbind",
        "rprivate",
    };

    // Common NVIDIA-related directories (best-effort).
    addMountIfExists("/sbin", kRoDirOptions);
    addMountIfExists("/usr/bin", kRoDirOptions);
    addMountIfExists("/run/nvidia-persistenced", kRoDirOptions, true);

    addMountIfExists("/usr/share/vulkan/icd.d", kRoDirOptions);
    addMountIfExists("/usr/share/vulkan/implicit_layer.d", kRoDirOptions);
    addMountIfExists("/usr/share/egl/egl_external_platform.d", kRoDirOptions);
    addMountIfExists("/usr/share/glvnd/egl_vendor.d", kRoDirOptions);

    addMountIfExists("/usr/share/nvidia", kRoDirOptions);
    addMountIfExists("/usr/share/X11/xorg.conf.d", kRoDirOptions);
    addMountIfExists("/usr/lib/xorg/modules/drivers", kRoDirOptions);

    // Library roots
    addMountIfExists("/usr/lib/x86_64-linux-gnu", kRoDirOptions);
    addMountIfExists("/usr/lib/x86_64-linux-gnu/nvidia/current", kRoDirOptions);
    addMountIfExists("/usr/lib/i386-linux-gnu", kRoDirOptions);
    addMountIfExists("/usr/lib/i386-linux-gnu/nvidia/current", kRoDirOptions);

    // Firmware directory (versioned). Best-effort by scanning /lib/firmware/nvidia/*
    {
        std::error_code ec;
        const std::filesystem::path fwRoot = "/lib/firmware/nvidia";
        if (std::filesystem::exists(fwRoot, ec) && std::filesystem::is_directory(fwRoot, ec)) {
            // Prefer the first versioned subdir that looks like digits.digits.digits
            for (const auto &entry : std::filesystem::directory_iterator(fwRoot, ec)) {
                if (ec) {
                    break;
                }
                if (!entry.is_directory(ec)) {
                    ec.clear();
                    continue;
                }
                addMountIfExists(entry.path(), kRoDirOptions);
                break;
            }
        }
    }

    // Devices: follow nvidia-ctk behavior: mount device nodes explicitly.
    auto addDevNodeIfExists = [&](const std::string &path) {
        std::error_code ec;
        if (!std::filesystem::exists(path, ec) || ec) {
            return;
        }
                edits.deviceNodes.push_back(api::types::v1::DeviceNode{
                    .hostPath = path,
                    .path = path,
                });
    };

    addDevNodeIfExists("/dev/nvidiactl");
    addDevNodeIfExists("/dev/nvidia0");
    addDevNodeIfExists("/dev/nvidia1");
    addDevNodeIfExists("/dev/nvidia-modeset");
    addDevNodeIfExists("/dev/nvidia-uvm");
    addDevNodeIfExists("/dev/nvidia-uvm-tools");

    {
        std::error_code ec;
        const std::filesystem::path driDir = "/dev/dri";
        if (std::filesystem::exists(driDir, ec) && std::filesystem::is_directory(driDir, ec)) {
            for (const auto &entry : std::filesystem::directory_iterator(driDir, ec)) {
                if (ec) {
                    break;
                }
                auto name = entry.path().filename().string();
                if (name.rfind("card", 0) == 0 || name.rfind("renderD", 0) == 0) {
                    addDevNodeIfExists(entry.path().string());
                }
            }
        }
    }

    // Match nvidia-ctk environment seen in CDI mode
    edits.env["NVIDIA_VISIBLE_DEVICES"] = "void";

    if (edits.empty()) {
        return LINGLONG_ERR("no NVIDIA files/devices found for builtin CDI fallback");
    }

    return edits;
}

} // namespace linglong::extension::cdi
