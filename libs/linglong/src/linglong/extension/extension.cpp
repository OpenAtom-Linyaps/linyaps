/*
 * SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "extension.h"

#include "linglong/common/strings.h"
#include "linglong/utils/log/log.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <unordered_map>
#include <unordered_set>

namespace linglong::extension {

namespace {

std::string toLower(std::string_view input)
{
    std::string out;
    out.reserve(input.size());
    for (unsigned char ch : input) {
        out.push_back(static_cast<char>(std::tolower(ch)));
    }
    return out;
}

bool isElf32(const std::filesystem::path &path)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream.is_open()) {
        return false;
    }

    std::array<unsigned char, 5> header{};
    stream.read(reinterpret_cast<char *>(header.data()), header.size());
    if (!stream || header[0] != 0x7f || header[1] != 'E' || header[2] != 'L' || header[3] != 'F') {
        return false;
    }

    return header[4] == 1;
}

struct NvidiaLibEntry
{
    std::filesystem::path source;
    std::filesystem::path linkPath;
    std::string name;
    bool is32{ false };
};

std::vector<NvidiaLibEntry> listNvidiaLibs()
{
    std::vector<NvidiaLibEntry> libs;
    std::unordered_set<std::string> seenNames;

    std::string cmd = "ldconfig -p";
    if (std::filesystem::exists("/sbin/ldconfig")) {
        cmd = "/sbin/ldconfig -p";
    } else if (std::filesystem::exists("/usr/sbin/ldconfig")) {
        cmd = "/usr/sbin/ldconfig -p";
    }

    auto *pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        LogW("failed to run ldconfig for NVIDIA driver discovery");
        return libs;
    }

    std::array<char, 4096> buffer{};
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        std::string line(buffer.data());
        auto lower = toLower(line);
        if (lower.find("nvidia") == std::string::npos && lower.find("libcuda") == std::string::npos) {
            continue;
        }

        auto pos = line.find("=>");
        if (pos == std::string::npos) {
            continue;
        }

        auto namePart = line.substr(0, pos);
        auto nameTrimmed = linglong::common::strings::trim(namePart, " \t\n");
        if (nameTrimmed.empty()) {
            continue;
        }
        auto paren = nameTrimmed.find('(');
        auto name = linglong::common::strings::trim(nameTrimmed.substr(0, paren), " \t\n");
        if (name.empty()) {
            continue;
        }

        auto path = linglong::common::strings::trim(line.substr(pos + 2), " \t\n");
        if (path.empty()) {
            continue;
        }

        std::error_code ec;
        auto resolved = std::filesystem::canonical(path, ec);
        if (ec) {
            continue;
        }

        bool is32 = isElf32(resolved);
        auto key = name + (is32 ? "#32" : "#64");
        if (!seenNames.insert(key).second) {
            continue;
        }

        libs.push_back(NvidiaLibEntry{
          .source = resolved,
          .linkPath = std::filesystem::path(path),
          .name = name,
          .is32 = is32,
        });
    }

    pclose(pipe);
    return libs;
}

std::vector<std::filesystem::path> collectVendorJsons(const std::filesystem::path &dir)
{
    std::vector<std::filesystem::path> results;
    std::error_code ec;
    if (!std::filesystem::exists(dir, ec)) {
        return results;
    }

    for (const auto &entry : std::filesystem::directory_iterator(dir, ec)) {
        if (ec) {
            break;
        }
        if (!entry.is_regular_file(ec)) {
            ec.clear();
            continue;
        }

        auto filename = entry.path().filename().string();
        auto lower = toLower(filename);
        if (lower.find("nvidia") == std::string::npos) {
            continue;
        }
        if (toLower(entry.path().extension().string()) != ".json") {
            continue;
        }

        results.push_back(entry.path());
    }

    std::sort(results.begin(), results.end());
    return results;
}

bool updateJsonLibraryPath(nlohmann::json &node,
                           const std::string &extensionName,
                           const std::unordered_map<std::string, bool> &libIs32,
                           std::unordered_set<std::string> &missingLibs)
{
    if (!node.is_object()) {
        return false;
    }

    auto it = node.find("library_path");
    if (it == node.end() || !it->is_string()) {
        return false;
    }

    std::string oldPath = it->get<std::string>();
    auto libName = std::filesystem::path(oldPath).filename().string();
    if (libName.empty()) {
        return false;
    }

    bool is32 = false;
    auto match = libIs32.find(libName);
    if (match == libIs32.end()) {
        auto oldPathFs = std::filesystem::path(oldPath);
        if (oldPathFs.is_absolute()) {
            missingLibs.insert(oldPathFs.string());
        }
        return true;
    }

    is32 = match->second;
    auto newPath = std::filesystem::path("/opt/extensions") / extensionName
      / (is32 ? "lib32" : "lib") / libName;
    *it = newPath.string();
    return true;
}

struct JsonWriteResult
{
    std::filesystem::path path;
    std::vector<std::filesystem::path> missingLibs;
    bool hasLibraryPath{ false };
};

std::optional<JsonWriteResult>
writeNvidiaJson(const std::filesystem::path &source,
                const std::filesystem::path &destination,
                const std::string &extensionName,
                const std::unordered_map<std::string, bool> &libIs32)
{
    std::ifstream input(source);
    if (!input.is_open()) {
        return std::nullopt;
    }

    nlohmann::json json;
    try {
        input >> json;
    } catch (const std::exception &ex) {
        LogW("failed to parse JSON {}: {}", source.string(), ex.what());
        std::error_code ec;
        if (!std::filesystem::copy_file(source,
                                        destination,
                                        std::filesystem::copy_options::overwrite_existing,
                                        ec)) {
            return std::nullopt;
        }
        return JsonWriteResult{
            .path = destination,
            .missingLibs = {},
            .hasLibraryPath = false,
        };
    }

    bool handled = false;
    std::unordered_set<std::string> missingLibs;
    if (json.contains("ICD")) {
        auto &icd = json["ICD"];
        if (icd.is_object()) {
            handled = updateJsonLibraryPath(icd, extensionName, libIs32, missingLibs) || handled;
        } else if (icd.is_array()) {
            for (auto &entry : icd) {
                handled = updateJsonLibraryPath(entry, extensionName, libIs32, missingLibs) || handled;
            }
        }
    }
    if (!handled) {
        handled = updateJsonLibraryPath(json, extensionName, libIs32, missingLibs);
    }

    if (!handled) {
        LogW("no library_path found in {}", source.string());
    }

    std::ofstream output(destination);
    if (!output.is_open()) {
        return std::nullopt;
    }

    output << json.dump(2);

    std::vector<std::filesystem::path> missing;
    missing.reserve(missingLibs.size());
    for (const auto &entry : missingLibs) {
        missing.emplace_back(entry);
    }

    return JsonWriteResult{
        .path = destination,
        .missingLibs = std::move(missing),
        .hasLibraryPath = handled,
    };
}

} // namespace

ExtensionIf::~ExtensionIf() { }

std::unique_ptr<ExtensionIf> ExtensionFactory::makeExtension(const std::string &name)
{
    if (name == ExtensionImplNVIDIADisplayDriver::Identify) {
        return std::make_unique<ExtensionImplNVIDIADisplayDriver>();
    }

    return std::make_unique<ExtensionImplDummy>();
}

ExtensionImplNVIDIADisplayDriver::ExtensionImplNVIDIADisplayDriver()
{
    driverName = hostDriverEnable();
}

bool ExtensionImplNVIDIADisplayDriver::shouldEnable(std::string &extensionName)
{
    if (extensionName != Identify) {
        return false;
    }

    if (!driverName.empty()) {
        extensionName = std::string(Identify) + "." + driverName;
        return true;
    }

    return false;
}

std::string ExtensionImplNVIDIADisplayDriver::hostDriverEnable()
{
    std::string version;

    std::ifstream versionFile("/sys/module/nvidia/version");
    if (versionFile) {
        versionFile >> version;

        std::replace(version.begin(), version.end(), '.', '-');
    }

    return version;
}

bool isNvidiaDisplayDriverExtension(const std::string &extensionName)
{
    const std::string prefix = ExtensionImplNVIDIADisplayDriver::Identify;
    if (extensionName == prefix) {
        return true;
    }
    if (extensionName.size() <= prefix.size()) {
        return false;
    }
    const auto prefixWithDot = prefix + ".";
    return extensionName.find(prefixWithDot, 0) == 0;
}

std::optional<HostExtensionInfo>
prepareHostNvidiaExtension(const std::filesystem::path &baseDir,
                           const std::string &extensionName)
{
    if (!isNvidiaDisplayDriverExtension(extensionName)) {
        return std::nullopt;
    }

    std::error_code ec;
    std::filesystem::create_directories(baseDir, ec);
    if (ec) {
        LogW("failed to create host extension base dir {}: {}", baseDir.string(), ec.message());
        return std::nullopt;
    }

    auto root = baseDir / extensionName;
    std::filesystem::create_directories(root, ec);
    if (ec) {
        LogW("failed to create host extension dir {}: {}", root.string(), ec.message());
        return std::nullopt;
    }

    HostExtensionInfo info{
        .root = root,
        .extraBinds = {},
        .vkIcdFile = std::nullopt,
        .eglExternalPlatformDir = std::nullopt,
        .eglVendorDir = std::nullopt,
    };

    bool hasLibs = false;
    bool has32Bit = false;
    std::unordered_map<std::string, bool> libIs32;
    auto libs = listNvidiaLibs();
    for (const auto &lib : libs) {
        if (lib.name.empty()) {
            continue;
        }

        auto libDir = lib.is32 ? (root / "lib32") : (root / "lib");
        std::filesystem::create_directories(libDir, ec);
        if (ec) {
            LogW("failed to create host NVIDIA lib dir {}: {}", libDir.string(), ec.message());
            ec.clear();
            continue;
        }

        auto destPath = libDir / lib.name;
        if (std::filesystem::is_symlink(destPath, ec) || (std::filesystem::exists(destPath, ec)
                                                          && !std::filesystem::is_regular_file(destPath, ec))) {
            std::filesystem::remove(destPath, ec);
            ec.clear();
        }
        if (!std::filesystem::exists(destPath, ec)) {
            std::ofstream placeholder(destPath);
        }

        auto destination = std::filesystem::path("/opt/extensions") / extensionName
          / (lib.is32 ? "lib32" : "lib") / lib.name;
        info.extraBinds.push_back(HostExtensionFileBind{
          .source = lib.source,
          .destination = destination,
        });

        auto it = libIs32.find(lib.name);
        if (it == libIs32.end()) {
            libIs32.emplace(lib.name, lib.is32);
        } else if (it->second && !lib.is32) {
            it->second = false;
        }
        hasLibs = true;
        has32Bit = has32Bit || lib.is32;
        if (lib.name.rfind("libGLX_nvidia", 0) == 0) {
            info.hasGlxLib = true;
        }
    }

    if (!hasLibs) {
        LogD("no NVIDIA driver libs found via ldconfig");
        return std::nullopt;
    }

    auto ldConfPath = root / "etc/ld.so.conf";
    std::filesystem::create_directories(ldConfPath.parent_path(), ec);
    if (!ec) {
        std::ofstream ldConf(ldConfPath);
        if (ldConf.is_open() && has32Bit) {
            ldConf << "lib32\n";
        }
    }

    const std::filesystem::path kVulkanIcd = "/usr/share/vulkan/icd.d/nvidia_icd.json";
    const std::filesystem::path kEglExternalDir = "/usr/share/egl/egl_external_platform.d";
    const std::filesystem::path kEglVendorDir = "/usr/share/glvnd/egl_vendor.d";
    const std::filesystem::path kGlxVendorDir = "/usr/share/glvnd/glx_vendor.d";
    const auto kEglExternalRel = kEglExternalDir.relative_path();
    const auto kEglVendorRel = kEglVendorDir.relative_path();
    const auto kGlxVendorRel = kGlxVendorDir.relative_path();

    std::unordered_set<std::string> extraBindTargets;
    extraBindTargets.reserve(libs.size() + 4);
    bool needsHostPathBinds = false;
    auto addMissingLibBinds =
      [&extraBindTargets, &info, &ec](const std::vector<std::filesystem::path> &missingLibs) {
          for (const auto &missingLib : missingLibs) {
              if (!missingLib.is_absolute()) {
                  continue;
              }
              auto normalized = missingLib.lexically_normal();
              if (!extraBindTargets.insert(normalized.string()).second) {
                  continue;
              }
              auto resolved = std::filesystem::canonical(normalized, ec);
              if (ec) {
                  ec.clear();
                  continue;
              }
              info.extraBinds.push_back(HostExtensionFileBind{
                .source = resolved,
                .destination = normalized,
              });
          }
      };
    auto addVendorJsons = [&](const std::filesystem::path &sourceDir,
                              const std::filesystem::path &destRelDir,
                              std::optional<std::filesystem::path> *exportDir) {
        for (const auto &vendorJson : collectVendorJsons(sourceDir)) {
            auto dest = root / destRelDir / vendorJson.filename();
            std::filesystem::create_directories(dest.parent_path(), ec);
            ec.clear();
            auto written = writeNvidiaJson(vendorJson, dest, extensionName, libIs32);
            if (!written) {
                continue;
            }
            info.extraBinds.push_back(HostExtensionFileBind{
              .source = written->path,
              .destination = vendorJson,
            });
            extraBindTargets.insert(vendorJson.string());
            if (exportDir && !*exportDir) {
                *exportDir =
                  std::filesystem::path("/opt/extensions") / extensionName / destRelDir;
            }
            if (!written->hasLibraryPath) {
                needsHostPathBinds = true;
            }
            addMissingLibBinds(written->missingLibs);
        }
    };

    if (std::filesystem::exists(kVulkanIcd, ec)) {
        ec.clear();
        auto dest = root / "usr/share/vulkan/icd.d/nvidia_icd.json";
        std::filesystem::create_directories(dest.parent_path(), ec);
        ec.clear();
        auto written = writeNvidiaJson(kVulkanIcd, dest, extensionName, libIs32);
        if (written) {
            info.extraBinds.push_back(HostExtensionFileBind{
              .source = written->path,
              .destination = kVulkanIcd,
            });
            extraBindTargets.insert(kVulkanIcd.string());
            info.vkIcdFile =
              std::filesystem::path("/opt/extensions") / extensionName / "usr/share/vulkan/icd.d/nvidia_icd.json";
            if (!written->hasLibraryPath) {
                needsHostPathBinds = true;
            }
            addMissingLibBinds(written->missingLibs);
        }
    }

    addVendorJsons(kEglExternalDir, kEglExternalRel, &info.eglExternalPlatformDir);
    addVendorJsons(kEglVendorDir, kEglVendorRel, &info.eglVendorDir);
    addVendorJsons(kGlxVendorDir, kGlxVendorRel, nullptr);

    if (needsHostPathBinds) {
        for (const auto &lib : libs) {
            if (!lib.linkPath.is_absolute()) {
                continue;
            }
            auto dest = lib.linkPath.lexically_normal();
            if (!extraBindTargets.insert(dest.string()).second) {
                continue;
            }
            info.extraBinds.push_back(HostExtensionFileBind{
              .source = lib.source,
              .destination = dest,
            });
        }
    }

    return info;
}

} // namespace linglong::extension
