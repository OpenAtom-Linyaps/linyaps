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
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <elf.h>
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

template <typename Ehdr, typename Phdr, typename Dyn>
std::optional<std::string> readElfSonameImpl(std::ifstream &file)
{
    Ehdr header{};
    file.seekg(0);
    file.read(reinterpret_cast<char *>(&header), sizeof(header));
    if (!file) {
        return std::nullopt;
    }
    if (header.e_phoff == 0 || header.e_phnum == 0) {
        return std::nullopt;
    }

    file.seekg(header.e_phoff);
    std::vector<Phdr> phdrs(header.e_phnum);
    file.read(reinterpret_cast<char *>(phdrs.data()), sizeof(Phdr) * phdrs.size());
    if (!file) {
        return std::nullopt;
    }

    std::optional<Phdr> dynamicPhdr;
    for (const auto &phdr : phdrs) {
        if (phdr.p_type == PT_DYNAMIC) {
            dynamicPhdr = phdr;
            break;
        }
    }
    if (!dynamicPhdr || dynamicPhdr->p_offset == 0 || dynamicPhdr->p_filesz == 0) {
        return std::nullopt;
    }

    file.seekg(dynamicPhdr->p_offset);
    size_t entryCount = dynamicPhdr->p_filesz / sizeof(Dyn);
    std::optional<std::uint64_t> sonameOffset;
    std::optional<std::uint64_t> strtabAddr;
    std::optional<std::uint64_t> strtabSize;
    for (size_t i = 0; i < entryCount; ++i) {
        Dyn entry{};
        file.read(reinterpret_cast<char *>(&entry), sizeof(entry));
        if (!file) {
            return std::nullopt;
        }
        if (entry.d_tag == DT_NULL) {
            break;
        }
        if (entry.d_tag == DT_SONAME) {
            sonameOffset = static_cast<std::uint64_t>(entry.d_un.d_val);
        } else if (entry.d_tag == DT_STRTAB) {
            strtabAddr = static_cast<std::uint64_t>(entry.d_un.d_ptr);
        } else if (entry.d_tag == DT_STRSZ) {
            strtabSize = static_cast<std::uint64_t>(entry.d_un.d_val);
        }
    }

    if (!sonameOffset || !strtabAddr || !strtabSize) {
        return std::nullopt;
    }

    std::optional<std::uint64_t> strtabOffset;
    for (const auto &phdr : phdrs) {
        if (phdr.p_type != PT_LOAD) {
            continue;
        }
        auto begin = static_cast<std::uint64_t>(phdr.p_vaddr);
        auto end = begin + static_cast<std::uint64_t>(phdr.p_memsz);
        auto addr = static_cast<std::uint64_t>(*strtabAddr);
        if (addr >= begin && addr < end) {
            strtabOffset = static_cast<std::uint64_t>(phdr.p_offset) + (addr - begin);
            break;
        }
    }
    if (!strtabOffset) {
        return std::nullopt;
    }

    std::vector<char> buffer(*strtabSize);
    file.seekg(*strtabOffset);
    file.read(buffer.data(), buffer.size());
    if (!file) {
        return std::nullopt;
    }

    if (*sonameOffset >= buffer.size()) {
        return std::nullopt;
    }

    const char *start = buffer.data() + *sonameOffset;
    size_t maxLen = buffer.size() - *sonameOffset;
    size_t len = strnlen(start, maxLen);
    if (len == 0 || len == maxLen) {
        return std::nullopt;
    }

    return std::string(start, len);
}

std::optional<std::string> readElfSoname(const std::filesystem::path &path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return std::nullopt;
    }

    unsigned char ident[EI_NIDENT]{};
    file.read(reinterpret_cast<char *>(ident), sizeof(ident));
    if (!file) {
        return std::nullopt;
    }
    if (ident[EI_MAG0] != ELFMAG0 || ident[EI_MAG1] != ELFMAG1 || ident[EI_MAG2] != ELFMAG2
        || ident[EI_MAG3] != ELFMAG3) {
        return std::nullopt;
    }
    if (ident[EI_DATA] != ELFDATA2LSB) {
        return std::nullopt;
    }

    if (ident[EI_CLASS] == ELFCLASS64) {
        return readElfSonameImpl<Elf64_Ehdr, Elf64_Phdr, Elf64_Dyn>(file);
    }
    if (ident[EI_CLASS] == ELFCLASS32) {
        return readElfSonameImpl<Elf32_Ehdr, Elf32_Phdr, Elf32_Dyn>(file);
    }
    return std::nullopt;
}


bool isSimpleSoName(const std::string &filename)
{
    auto pos = filename.find(".so");
    if (pos == std::string::npos) {
        return false;
    }
    pos += 3;
    if (pos == filename.size()) {
        return true;
    }
    if (filename[pos] != '.') {
        return false;
    }
    for (size_t i = pos + 1; i < filename.size(); ++i) {
        if (!std::isdigit(static_cast<unsigned char>(filename[i]))) {
            return false;
        }
    }
    return true;
}

struct NvidiaLibEntry
{
    std::filesystem::path source;
    std::string name;
    std::vector<std::string> aliases;
    bool is32{ false };
};

std::vector<NvidiaLibEntry> listNvidiaLibs()
{
    std::vector<NvidiaLibEntry> libs;
    std::unordered_map<std::string, size_t> entryIndex;

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

        auto soname = readElfSoname(resolved);
        std::string canonicalName = name;
        if (soname && !soname->empty()) {
            canonicalName = *soname;
        }

        bool is32 = isElf32(resolved);
        auto key = canonicalName + (is32 ? "#32" : "#64");
        auto existing = entryIndex.find(key);
        if (existing != entryIndex.end()) {
            if (name != canonicalName) {
                auto &aliases = libs[existing->second].aliases;
                if (std::find(aliases.begin(), aliases.end(), name) == aliases.end()) {
                    aliases.push_back(name);
                }
            }
            continue;
        }

        NvidiaLibEntry entry{
          .source = resolved,
          .name = canonicalName,
          .aliases = {},
          .is32 = is32,
        };
        if (name != canonicalName) {
            entry.aliases.push_back(name);
        }
        libs.push_back(std::move(entry));
        entryIndex.emplace(key, libs.size() - 1);
    }

    pclose(pipe);

    std::unordered_set<std::string> extraDirs;
    extraDirs.reserve(libs.size());
    for (const auto &lib : libs) {
        extraDirs.insert(lib.source.parent_path().string());
    }

    for (const auto &dir : extraDirs) {
        std::error_code ec;
        for (const auto &dirEntry : std::filesystem::directory_iterator(dir, ec)) {
            if (ec) {
                break;
            }
            if (!dirEntry.is_regular_file(ec) && !dirEntry.is_symlink(ec)) {
                ec.clear();
                continue;
            }
            auto filename = dirEntry.path().filename().string();
            auto lower = toLower(filename);
            if (lower.find("nvidia") == std::string::npos && lower.find("libcuda") == std::string::npos) {
                continue;
            }
            if (filename.find(".so") == std::string::npos) {
                continue;
            }
            if (!isSimpleSoName(filename)) {
                continue;
            }
            auto resolved = std::filesystem::canonical(dirEntry.path(), ec);
            if (ec) {
                ec.clear();
                continue;
            }
            auto soname = readElfSoname(resolved);
            std::string name = soname.value_or(filename);
            auto nameLower = toLower(name);
            if (nameLower.find("nvidia") == std::string::npos
                && nameLower.find("libcuda") == std::string::npos) {
                continue;
            }
            bool is32 = isElf32(resolved);
            auto key = name + (is32 ? "#32" : "#64");
            auto existing = entryIndex.find(key);
            if (existing != entryIndex.end()) {
                if (filename != name) {
                    auto &aliases = libs[existing->second].aliases;
                    if (std::find(aliases.begin(), aliases.end(), filename) == aliases.end()) {
                        aliases.push_back(filename);
                    }
                }
                continue;
            }
            NvidiaLibEntry libEntry{
              .source = resolved,
              .name = name,
              .aliases = {},
              .is32 = is32,
            };
            if (filename != name) {
                libEntry.aliases.push_back(filename);
            }
            libs.push_back(std::move(libEntry));
            entryIndex.emplace(key, libs.size() - 1);
        }
    }

    return libs;
}

std::optional<std::filesystem::path>
findNvidiaSmiPath(const std::vector<NvidiaLibEntry> &libs)
{
    std::vector<std::filesystem::path> candidates = {
        "/usr/bin/nvidia-smi",
        "/usr/sbin/nvidia-smi",
        "/sbin/nvidia-smi",
        "/usr/local/bin/nvidia-smi",
    };

    std::unordered_set<std::string> seen;
    for (const auto &candidate : candidates) {
        seen.insert(candidate);
    }

    for (const auto &lib : libs) {
        auto dir = lib.source.parent_path();
        if (!dir.empty()) {
            auto cand = dir / "nvidia-smi";
            if (seen.insert(cand.string()).second) {
                candidates.push_back(cand);
            }
        }
    }

    for (const auto &candidate : candidates) {
        std::error_code ec;
        if (std::filesystem::exists(candidate, ec) && std::filesystem::is_regular_file(candidate, ec)) {
            return candidate;
        }
    }

    return std::nullopt;
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
    auto oldPathFs = std::filesystem::path(oldPath);
    auto libName = oldPathFs.filename().string();
    if (libName.empty()) {
        return false;
    }

    std::string resolvedName = libName;
    if (oldPathFs.is_absolute()) {
        if (auto soname = readElfSoname(oldPathFs)) {
            if (!soname->empty()) {
                resolvedName = *soname;
            }
        }
    }

    bool is32 = false;
    auto match = libIs32.find(resolvedName);
    if (match == libIs32.end()) {
        if (oldPathFs.is_absolute()) {
            std::error_code ec;
            if (std::filesystem::exists(oldPathFs, ec) && !ec) {
                is32 = isElf32(oldPathFs);
            }
            missingLibs.insert(oldPathFs.string());
        }
        auto newPath = std::filesystem::path("/opt/extensions") / extensionName
          / (is32 ? "orig/32" : "orig") / resolvedName;
        *it = newPath.string();
        return true;
    }

    is32 = match->second;
    auto newPath = std::filesystem::path("/opt/extensions") / extensionName
      / (is32 ? "orig/32" : "orig") / resolvedName;
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
    std::unordered_set<std::string> extraBindTargets;
    extraBindTargets.reserve(2048);
    auto ensureFile = [&](const std::filesystem::path &path) {
        if (path.empty()) {
            return;
        }
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        ec.clear();
        if (std::filesystem::exists(path, ec)) {
            if (std::filesystem::is_directory(path, ec)) {
                ec.clear();
                std::filesystem::remove_all(path, ec);
            }
        }
        ec.clear();
        std::ofstream ofs(path, std::ios::binary | std::ios::out | std::ios::app);
    };
    auto bindHostFile = [&](const std::filesystem::path &destRel,
                            const std::filesystem::path &source) {
        if (destRel.empty() || source.empty() || !source.is_absolute()) {
            return;
        }
        auto destInRoot = root / destRel;
        ensureFile(destInRoot);

        auto destInContainer = std::filesystem::path("/opt/extensions") / extensionName / destRel;
        if (extraBindTargets.insert(destInContainer.string()).second) {
            info.extraBinds.push_back(HostExtensionFileBind{
              .source = source,
              .destination = destInContainer,
            });
        }
    };

    bool hasLibs = false;
    bool has32Bit = false;
    std::unordered_map<std::string, bool> libIs32;
    auto libs = listNvidiaLibs();
    for (const auto &lib : libs) {
        if (lib.name.empty()) {
            continue;
        }

        auto libDir = lib.is32 ? (root / "orig/32") : (root / "orig");
        std::filesystem::create_directories(libDir, ec);
        if (ec) {
            LogW("failed to create host NVIDIA lib dir {}: {}", libDir.string(), ec.message());
            ec.clear();
            continue;
        }

        auto destRelDir = lib.is32 ? std::filesystem::path("orig/32") : std::filesystem::path("orig");
        auto destRelPath = destRelDir / lib.name;
        bindHostFile(destRelPath, lib.source);

        for (const auto &alias : lib.aliases) {
            if (alias.empty() || alias == lib.name) {
                continue;
            }
            auto aliasPath = libDir / alias;
            if (std::filesystem::is_symlink(aliasPath, ec)) {
                auto target = std::filesystem::read_symlink(aliasPath, ec);
                if (!ec && target == std::filesystem::path(lib.name)) {
                    ec.clear();
                    continue;
                }
            }
            ec.clear();
            if (std::filesystem::exists(aliasPath, ec)) {
                std::filesystem::remove(aliasPath, ec);
            }
            ec.clear();
            std::filesystem::create_symlink(lib.name, aliasPath, ec);
            ec.clear();
        }


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

    if (auto nvidiaSmi = findNvidiaSmiPath(libs)) {
        std::filesystem::path rel = std::filesystem::path("usr/bin/nvidia-smi");
        bindHostFile(rel, *nvidiaSmi);
        auto rootDest = std::filesystem::path("/usr/bin/nvidia-smi");
        if (extraBindTargets.insert(rootDest.string()).second) {
            info.extraBinds.push_back(HostExtensionFileBind{
              .source = *nvidiaSmi,
              .destination = rootDest,
            });
        }
    }

    auto ldConfPath = root / "etc/ld.so.conf";
    std::filesystem::create_directories(ldConfPath.parent_path(), ec);
    if (!ec) {
        std::ofstream ldConf(ldConfPath);
        if (ldConf.is_open()) {
            ldConf << "/opt/extensions/" << extensionName << "/orig\n";
            if (has32Bit) {
                ldConf << "/opt/extensions/" << extensionName << "/orig/32\n";
            }
        }
    }

    const std::array<std::filesystem::path, 2> kVulkanIcdCandidates = {
        std::filesystem::path{ "/etc/vulkan/icd.d/nvidia_icd.json" },
        std::filesystem::path{ "/usr/share/vulkan/icd.d/nvidia_icd.json" },
    };
    const std::filesystem::path kEglExternalDir = "/usr/share/egl/egl_external_platform.d";
    const std::filesystem::path kEglVendorDir = "/usr/share/glvnd/egl_vendor.d";
    const std::filesystem::path kGlxVendorDir = "/usr/share/glvnd/glx_vendor.d";
    const auto kEglExternalRel = kEglExternalDir.relative_path();
    const auto kEglVendorRel = kEglVendorDir.relative_path();
    const auto kGlxVendorRel = kGlxVendorDir.relative_path();

    auto addMissingLibBinds =
      [&bindHostFile](const std::vector<std::filesystem::path> &missingLibs) {
          for (const auto &missingLib : missingLibs) {
              if (!missingLib.is_absolute()) {
                  continue;
              }
              auto normalized = missingLib.lexically_normal();
              if (!normalized.has_filename()) {
                  continue;
              }

              std::error_code ec;
              auto resolved = std::filesystem::canonical(normalized, ec);
              if (ec) {
                  ec.clear();
                  continue;
              }

              bool is32 = isElf32(resolved);
              std::string destName = normalized.filename().string();
              if (auto soname = readElfSoname(resolved)) {
                  if (!soname->empty()) {
                      destName = *soname;
                  }
              }
              auto relDir = is32 ? "orig/32" : "orig";
              auto destRel = std::filesystem::path(relDir) / destName;
              bindHostFile(destRel, resolved);
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
            if (exportDir && !*exportDir) {
                *exportDir =
                  std::filesystem::path("/opt/extensions") / extensionName / destRelDir;
            }
            if (!written->hasLibraryPath) {
                LogW("no library_path found in {}", vendorJson.string());
            }
            addMissingLibBinds(written->missingLibs);
        }
    };

    for (const auto &kVulkanIcd : kVulkanIcdCandidates) {
        if (!std::filesystem::exists(kVulkanIcd, ec)) {
            ec.clear();
            continue;
        }

        ec.clear();
        auto dest = root / "etc/vulkan/icd.d/nvidia_icd.json";
        std::filesystem::create_directories(dest.parent_path(), ec);
        ec.clear();
        auto written = writeNvidiaJson(kVulkanIcd, dest, extensionName, libIs32);
        if (written) {
            info.vkIcdFile =
              std::filesystem::path("/opt/extensions") / extensionName / "etc/vulkan/icd.d/nvidia_icd.json";
            if (!written->hasLibraryPath) {
                LogW("no library_path found in {}", kVulkanIcd.string());
            }
            addMissingLibBinds(written->missingLibs);
        }
        break;
    }

    addVendorJsons(kEglExternalDir, kEglExternalRel, &info.eglExternalPlatformDir);
    addVendorJsons(kEglVendorDir, kEglVendorRel, &info.eglVendorDir);
    addVendorJsons(kGlxVendorDir, kGlxVendorRel, nullptr);

    return info;
}

} // namespace linglong::extension
