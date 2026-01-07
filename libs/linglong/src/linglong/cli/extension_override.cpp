/*
 * SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/cli/extension_override.h"

#include "linglong/extension/cdi.h"
#include "linglong/extension/extension.h"
#include "linglong/utils/command/cmd.h"

#include <nlohmann/json.hpp>

#include <cctype>
#include <elf.h>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <array>
#include <cstdio>
#include <fstream>
#include <unordered_set>
#include <vector>
#include <sstream>
#include <unordered_map>

namespace linglong::cli::extension_override {

namespace {

using linglong::api::types::v1::DeviceNode;
using linglong::runtime::ExtensionOverride;
using linglong::extension::cdi::ContainerEdits;


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



std::string trim(const std::string &input)
{
    auto start = input.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) {
        return {};
    }
    auto end = input.find_last_not_of(" \t\n\r");
    return input.substr(start, end - start + 1);
}

bool isVendorLibraryName(const std::string &name)
{
    if (name.empty()) {
        return false;
    }
    if (name.rfind("libnvme", 0) == 0) {
        return false;
    }
    if (name.rfind("libcuda", 0) == 0) {
        return true;
    }
    if (name.rfind("libnv", 0) == 0) {
        return true;
    }
    return name.find("nvidia") != std::string::npos;
}

std::unordered_map<std::string, std::vector<std::filesystem::path>> readLdconfigCache()
{
    std::unordered_map<std::string, std::vector<std::filesystem::path>> cache;
    linglong::utils::command::Cmd cmd("ldconfig");
    if (!cmd.exists()) {
        return cache;
    }
    auto output = cmd.exec(QStringList{ "-p" });
    if (!output) {
        return cache;
    }
    std::stringstream stream(output->toStdString());
    std::string line;
    while (std::getline(stream, line)) {
        auto arrow = line.find("=>");
        if (arrow == std::string::npos) {
            continue;
        }
        auto left = trim(line.substr(0, arrow));
        auto right = trim(line.substr(arrow + 2));
        if (left.empty() || right.empty()) {
            continue;
        }
        auto space = left.find(' ');
        std::string name = (space == std::string::npos) ? left : left.substr(0, space);
        if (name.empty()) {
            continue;
        }
        cache[name].push_back(std::filesystem::path(right));
    }
    return cache;
}

bool isPathUnder(const std::filesystem::path &path, const std::filesystem::path &dir)
{
    auto normalizedPath = path.lexically_normal().generic_string();
    auto normalizedDir = dir.lexically_normal().generic_string();
    if (normalizedPath == normalizedDir) {
        return true;
    }
    if (!normalizedDir.empty() && normalizedDir.back() != '/') {
        normalizedDir.push_back('/');
    }
    return normalizedPath.rfind(normalizedDir, 0) == 0;
}

std::string resolveExtensionMountName(const std::string &name)
{
    if (name != linglong::extension::ExtensionImplNVIDIADisplayDriver::Identify) {
        return name;
    }

    linglong::extension::ExtensionImplNVIDIADisplayDriver impl;
    std::string resolved = name;
    if (impl.shouldEnable(resolved)) {
        return resolved;
    }
    return name;
}


utils::error::Result<nlohmann::json> loadJsonFile(const std::filesystem::path &path)
{
    LINGLONG_TRACE("load extension override json file");

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

utils::error::Result<nlohmann::json> buildOverrideFromEdits(const ContainerEdits &edits,
                                                            const std::string &name,
                                                            bool fallbackOnly)
{
    LINGLONG_TRACE("build override from CDI edits");

    if (name.empty()) {
        return LINGLONG_ERR("extension name is empty");
    }

    nlohmann::json overrideJson = nlohmann::json::object();
    overrideJson["name"] = name;
    overrideJson["fallback_only"] = fallbackOnly;
    auto env = edits.env;
    auto setEnvIfEmpty = [](std::map<std::string, std::string> &envMap,
                            const std::string &key,
                            const std::string &value) {
        if (value.empty()) {
            return;
        }
        auto it = envMap.find(key);
        if (it != envMap.end() && !it->second.empty()) {
            return;
        }
        envMap[key] = value;
    };
    auto appendEnvPath = [](std::map<std::string, std::string> &envMap,
                            const std::string &key,
                            const std::vector<std::string> &paths) {
        if (paths.empty()) {
            return;
        }
        std::vector<std::string> ordered;
        std::unordered_set<std::string> seen;
        for (const auto &path : paths) {
            if (!path.empty() && seen.insert(path).second) {
                ordered.push_back(path);
            }
        }

        auto it = envMap.find(key);
        if (it != envMap.end() && !it->second.empty()) {
            const auto &current = it->second;
            size_t start = 0;
            while (start <= current.size()) {
                size_t end = current.find(':', start);
                std::string segment = (end == std::string::npos)
                  ? current.substr(start)
                  : current.substr(start, end - start);
                if (!segment.empty() && seen.insert(segment).second) {
                    ordered.push_back(segment);
                }
                if (end == std::string::npos) {
                    break;
                }
                start = end + 1;
            }
        }

        if (ordered.empty()) {
            return;
        }
        std::string merged;
        for (size_t i = 0; i < ordered.size(); ++i) {
            if (i) {
                merged.push_back(':');
            }
            merged.append(ordered[i]);
        }
        envMap[key] = std::move(merged);
    };
    if (!edits.mounts.empty()) {
        const std::string mountName = resolveExtensionMountName(name);
        const std::filesystem::path prefix = std::filesystem::path("/opt/extensions") / mountName;
        std::vector<std::filesystem::path> candidateDirs;
        std::unordered_set<std::string> candidateDirSet;
        auto addCandidateDir = [&](const std::filesystem::path &dir) {
            if (dir.empty()) {
                return;
            }
            auto value = dir.lexically_normal().string();
            if (candidateDirSet.insert(value).second) {
                candidateDirs.push_back(dir);
            }
        };
        auto appendUnique = [](std::unordered_set<std::string> &seen,
                               std::vector<std::string> &ordered,
                               const std::string &value) {
            if (!value.empty() && seen.insert(value).second) {
                ordered.push_back(value);
            }
        };
        auto isUnderOptExtensions = [](const std::filesystem::path &path) {
            const std::string prefix = "/opt/extensions/";
            auto value = path.generic_string();
            return value.rfind(prefix, 0) == 0;
        };
        auto isLibraryCandidate = [](const std::filesystem::path &path) {
            auto filename = path.filename().string();
            if (filename.rfind("lib", 0) == 0 && filename.find(".so") != std::string::npos) {
                return true;
            }
            return false;
        };

        auto mapLibraryDestination = [&](const std::string &source)
          -> std::optional<std::filesystem::path> {
            if (source.empty()) {
                return std::nullopt;
            }
            std::filesystem::path sourcePath{ source };
            std::error_code ec;
            if (!std::filesystem::is_regular_file(sourcePath, ec)
                && !std::filesystem::is_symlink(sourcePath, ec)) {
                return std::nullopt;
            }
            if (!isLibraryCandidate(sourcePath)) {
                return std::nullopt;
            }
            auto filename = sourcePath.filename();
            if (filename.empty()) {
                return std::nullopt;
            }
            std::string destName = filename.string();
            if (auto soname = readElfSoname(sourcePath)) {
                if (!soname->empty()) {
                    destName = *soname;
                }
            }
            bool is32 = isElf32(sourcePath);
            auto destDir = prefix / (is32 ? "orig/32" : "orig");
            return destDir / destName;
        };

        nlohmann::json mounts = nlohmann::json::array();
        std::unordered_set<std::string> mountKeySet;
        std::unordered_set<std::string> mountDestSet;
        std::unordered_set<std::string> libDirSet;
        std::unordered_set<std::string> eglExternalDirSet;
        std::unordered_set<std::string> eglVendorDirSet;
        std::unordered_set<std::string> vkIcdFileSet;
        std::optional<std::string> nvidiaSmiSource;
        std::vector<std::string> libDirs;
        std::vector<std::string> eglExternalDirs;
        std::vector<std::string> eglVendorDirs;
        std::vector<std::string> vkIcdFiles;
        bool hasGlxLib = false;
        auto addMount = [&](const std::filesystem::path &destination,
                            const std::string &source,
                            const std::optional<std::string> &type,
                            const std::optional<std::vector<std::string>> &options) {
            if (source.empty()) {
                return;
            }
            auto destKey = destination.string();
            if (!mountDestSet.insert(destKey).second) {
                return;
            }
            std::string key = destKey + "|" + source;
            if (!mountKeySet.insert(key).second) {
                return;
            }
            nlohmann::json mountJson = nlohmann::json::object();
            mountJson["source"] = source;
            mountJson["destination"] = destination.string();
            if (type && !type->empty()) {
                mountJson["type"] = *type;
            }
            if (options && !options->empty()) {
                mountJson["options"] = *options;
            }
            mounts.push_back(std::move(mountJson));
        };

        auto isRegularOrSymlink = [](const std::filesystem::path &path) {
            std::error_code ec;
            return std::filesystem::is_regular_file(path, ec) || std::filesystem::is_symlink(path, ec);
        };


        auto findNvidiaSmiFallback = [&](const std::vector<std::filesystem::path> &dirs)
          -> std::optional<std::string> {
            std::vector<std::filesystem::path> candidates;
            std::unordered_set<std::string> seen;
            auto addCandidate = [&](const std::filesystem::path &path) {
                if (path.empty()) {
                    return;
                }
                auto value = path.lexically_normal().string();
                if (seen.insert(value).second) {
                    candidates.push_back(path);
                }
            };

            addCandidate("/usr/bin/nvidia-smi");
            addCandidate("/usr/sbin/nvidia-smi");
            addCandidate("/sbin/nvidia-smi");
            addCandidate("/usr/local/bin/nvidia-smi");

            for (const auto &dir : dirs) {
                addCandidate(dir / "nvidia-smi");
            }

            for (const auto &candidate : candidates) {
                if (isRegularOrSymlink(candidate)) {
                    return candidate.string();
                }
            }

            return std::nullopt;
        };

        auto considerSourcePath = [&](const std::string &source) {
            if (source.empty()) {
                return;
            }
            std::filesystem::path sourcePath{ source };
            if (sourcePath.filename() == "nvidia-smi") {
                if (isRegularOrSymlink(sourcePath)) {
                    nvidiaSmiSource = source;
                }
            }
            std::error_code ec;
            if (std::filesystem::is_directory(sourcePath, ec)) {
                addCandidateDir(sourcePath);
                return;
            }
            if (!isRegularOrSymlink(sourcePath)) {
                return;
            }
            addCandidateDir(sourcePath.parent_path());
        };

        auto recordMount = [&](const std::filesystem::path &destPath,
                               const std::string &source,
                               const std::optional<std::string> &type,
                               const std::optional<std::vector<std::string>> &options) {
            addMount(destPath, source, type, options);

            bool underOptExtensions = isUnderOptExtensions(destPath);
            if (underOptExtensions && isLibraryCandidate(destPath)) {
                auto dir = destPath.parent_path().string();
                appendUnique(libDirSet, libDirs, dir);
                auto filename = destPath.filename().string();
                if (filename.rfind("libGLX_nvidia", 0) == 0) {
                    hasGlxLib = true;
                }
            }

            if (underOptExtensions) {
                auto parent = destPath.parent_path();
                if (parent.filename() == "egl_external_platform.d") {
                    appendUnique(eglExternalDirSet, eglExternalDirs, parent.string());
                } else if (parent.filename() == "egl_vendor.d") {
                    appendUnique(eglVendorDirSet, eglVendorDirs, parent.string());
                } else if (parent.filename() == "icd.d"
                           && parent.parent_path().filename() == "vulkan") {
                    appendUnique(vkIcdFileSet, vkIcdFiles, destPath.string());
                }
            }
        };

        for (const auto &mount : edits.mounts) {
            std::string source = mount.source.value_or("");
            considerSourcePath(source);
            std::filesystem::path destPath{ mount.destination };
            if (!mount.destination.empty()) {
                if (auto mapped = mapLibraryDestination(source)) {
                    destPath = *mapped;
                } else if (!isUnderOptExtensions(destPath) && destPath.is_absolute()) {
                    std::filesystem::path remapRel = destPath.relative_path();
                    auto normalized = destPath.lexically_normal();
                    if (normalized == "/usr/lib"
                        || normalized.generic_string().rfind("/usr/lib/", 0) == 0) {
                        auto rel = normalized.lexically_relative("/usr/lib");
                        if (!rel.empty() && rel.native() != "..") {
                            remapRel = rel;
                        }
                    }
                    destPath = prefix / remapRel;
                }
            }
            recordMount(destPath, source, mount.type, mount.options);
        }


        if (!nvidiaSmiSource && extension::isNvidiaDisplayDriverExtension(name)) {
            nvidiaSmiSource = findNvidiaSmiFallback(candidateDirs);
        }

        std::vector<std::filesystem::path> vendorLibPaths;
        std::unordered_set<std::string> vendorLibSet;
        auto addVendorLib = [&](const std::filesystem::path &path) {
            if (path.empty()) {
                return;
            }
            auto key = path.lexically_normal().string();
            if (!vendorLibSet.insert(key).second) {
                return;
            }
            vendorLibPaths.push_back(path);
        };
        auto ldconfigCache = readLdconfigCache();
        if (!ldconfigCache.empty()) {
            for (const auto &entry : ldconfigCache) {
                if (!isVendorLibraryName(entry.first)) {
                    continue;
                }
                for (const auto &path : entry.second) {
                    for (const auto &dir : candidateDirs) {
                        if (isPathUnder(path, dir)) {
                            addVendorLib(path);
                            break;
                        }
                    }
                }
            }
        }
        if (vendorLibPaths.empty()) {
            for (const auto &dir : candidateDirs) {
                std::error_code ec;
                for (const auto &entry : std::filesystem::directory_iterator(dir, ec)) {
                    if (ec) {
                        break;
                    }
                    const auto &path = entry.path();
                    if (!isLibraryCandidate(path)) {
                        continue;
                    }
                    if (!isVendorLibraryName(path.filename().string())) {
                        continue;
                    }
                    if (isRegularOrSymlink(path)) {
                        addVendorLib(path);
                    }
                }
            }
        }

        const std::optional<std::vector<std::string>> vendorOptions =
          std::vector<std::string>{ "rbind", "ro" };
        if (nvidiaSmiSource) {
            recordMount(std::filesystem::path("/usr/bin/nvidia-smi"),
                        *nvidiaSmiSource,
                        std::nullopt,
                        vendorOptions);
        }
        for (const auto &path : vendorLibPaths) {
            if (auto mapped = mapLibraryDestination(path.string())) {
                recordMount(*mapped, path.string(), std::nullopt, vendorOptions);
            }
        }

        overrideJson["mounts"] = std::move(mounts);
        appendEnvPath(env, "LD_LIBRARY_PATH", libDirs);
        appendEnvPath(env, "EGL_EXTERNAL_PLATFORM_CONFIG_DIRS", eglExternalDirs);
        appendEnvPath(env, "__EGL_EXTERNAL_PLATFORM_CONFIG_DIRS", eglExternalDirs);
        appendEnvPath(env, "__EGL_VENDOR_LIBRARY_DIRS", eglVendorDirs);
        appendEnvPath(env, "VK_ICD_FILENAMES", vkIcdFiles);
        appendEnvPath(env, "VK_ADD_DRIVER_FILES", vkIcdFiles);
        if (!libDirs.empty()) {
            env["NVIDIA_CTK_LIBCUDA_DIR"] = (prefix / "orig").string();
        }
        if (hasGlxLib) {
            setEnvIfEmpty(env, "__GLX_VENDOR_LIBRARY_NAME", "nvidia");
            setEnvIfEmpty(env, "__NV_PRIME_RENDER_OFFLOAD", "1");
        }
    }
    if (!edits.deviceNodes.empty()) {
        nlohmann::json nodes = nlohmann::json::array();
        for (const auto &node : edits.deviceNodes) {
            nlohmann::json nodeJson = nlohmann::json::object();
            nodeJson["path"] = node.path;
            if (node.hostPath) {
                nodeJson["hostPath"] = *node.hostPath;
            }
            nodes.push_back(std::move(nodeJson));
        }
        overrideJson["device_nodes"] = std::move(nodes);
    }

    if (!env.empty()) {
        overrideJson["env"] = std::move(env);
    }

    return overrideJson;
}

std::vector<ExtensionOverride> parseOverridesFromRoot(const nlohmann::json &root)
{
    std::vector<ExtensionOverride> overrides;
    auto overridesJson = root.find("extension_overrides");
    if (overridesJson == root.end() || !overridesJson->is_array()) {
        return overrides;
    }

    for (const auto &item : *overridesJson) {
        if (!item.is_object()) {
            continue;
        }

        std::string name = item.value("name", "");
        if (name.empty()) {
            continue;
        }

        ExtensionOverride overrideDef{
            .name = std::move(name),
            .fallbackOnly = item.value("fallback_only", false),
        };

        auto env = item.find("env");
        if (env != item.end() && env->is_object()) {
            for (auto it = env->begin(); it != env->end(); ++it) {
                if (it.value().is_string()) {
                    overrideDef.env.emplace(it.key(), it.value().get<std::string>());
                }
            }
        }

        auto mounts = item.find("mounts");
        if (mounts != item.end() && mounts->is_array()) {
            for (const auto &mountItem : *mounts) {
                if (!mountItem.is_object()) {
                    continue;
                }
                std::string source = mountItem.value("source", "");
                std::string destination = mountItem.value("destination", "");
                if (source.empty() || destination.empty()) {
                    continue;
                }

                std::vector<std::string> options;
                auto optionsJson = mountItem.find("options");
                if (optionsJson != mountItem.end()) {
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

                ocppi::runtime::config::types::Mount mount{
                    .destination = destination,
                    .options = options,
                    .source = source,
                    .type = mountItem.value("type", "bind"),
                };
                overrideDef.mounts.push_back(std::move(mount));
            }
        }

        auto deviceNodes = item.find("device_nodes");
        if (deviceNodes != item.end() && deviceNodes->is_array()) {
            for (const auto &nodeItem : *deviceNodes) {
                if (!nodeItem.is_object()) {
                    continue;
                }
                std::string path = nodeItem.value("path", "");
                if (path.empty()) {
                    continue;
                }

                std::optional<std::string> hostPath;
                auto hostPathValue = nodeItem.find("hostPath");
                if (hostPathValue != nodeItem.end() && hostPathValue->is_string()) {
                    hostPath = hostPathValue->get<std::string>();
                } else {
                    auto hostPathAlt = nodeItem.find("host_path");
                    if (hostPathAlt != nodeItem.end() && hostPathAlt->is_string()) {
                        hostPath = hostPathAlt->get<std::string>();
                    }
                }

                overrideDef.deviceNodes.emplace_back(DeviceNode{
                  .hostPath = hostPath,
                  .path = path,
                });
            }
        }

        overrides.push_back(std::move(overrideDef));
    }

    return overrides;
}

} // namespace

std::optional<std::filesystem::path> getUserConfigPath() noexcept
{
    const char *configHome = std::getenv("XDG_CONFIG_HOME");
    std::filesystem::path base;
    if (configHome && configHome[0] != '\0') {
        base = std::filesystem::path{ configHome };
    } else {
        const char *home = std::getenv("HOME");
        if (!home || home[0] == '\0') {
            return std::nullopt;
        }
        base = std::filesystem::path{ home } / ".config";
    }
    return base / "linglong" / "config.json";
}

utils::error::Result<std::vector<runtime::ExtensionOverride>>
loadOverrides(const std::filesystem::path &configPath) noexcept
{
    LINGLONG_TRACE("load extension overrides");

    std::error_code ec;
    if (!std::filesystem::exists(configPath, ec) || ec) {
        return std::vector<runtime::ExtensionOverride>{};
    }

    auto root = loadJsonFile(configPath);
    if (!root) {
        return LINGLONG_ERR(root.error());
    }

    if (!root->is_object()) {
        return std::vector<runtime::ExtensionOverride>{};
    }

    return parseOverridesFromRoot(*root);
}

utils::error::Result<std::vector<runtime::ExtensionOverride>>
loadCdiOverrides(const std::optional<std::filesystem::path> &cdiPath,
                 const std::string &name,
                 bool fallbackOnly) noexcept
{
    LINGLONG_TRACE("load CDI overrides");

    auto edits = cdiPath ? linglong::extension::cdi::loadFromFile(*cdiPath)
                         : linglong::extension::cdi::loadFromNvidiaCtk();
    if (!edits) {
        return LINGLONG_ERR(edits.error());
    }

    auto overrideJson = buildOverrideFromEdits(*edits, name, fallbackOnly);
    if (!overrideJson) {
        return LINGLONG_ERR(overrideJson.error());
    }

    nlohmann::json root = nlohmann::json::object();
    root["extension_overrides"] = nlohmann::json::array({ *overrideJson });
    return parseOverridesFromRoot(root);
}

utils::error::Result<void> importCdiOverrides(const std::filesystem::path &configPath,
                                              const std::optional<std::filesystem::path> &cdiPath,
                                              const std::string &name,
                                              bool fallbackOnly) noexcept
{
    LINGLONG_TRACE("import CDI overrides");

    utils::error::Result<linglong::extension::cdi::ContainerEdits> edits = LINGLONG_OK;
    if (cdiPath) {
        edits = linglong::extension::cdi::loadFromFile(*cdiPath);
    } else {
        edits = linglong::extension::cdi::loadFromNvidiaCtk();
        if (!edits) {
            // nvidia-ctk is optional; fallback to builtin host discovery.
            if (std::string(edits.error().message()).find("nvidia-ctk not found")
                != std::string::npos) {
                edits = linglong::extension::cdi::loadFromHostNvidia();
            }
        }
    }
    if (!edits) {
        return LINGLONG_ERR(edits.error());
    }

    auto overrideItem = buildOverrideFromEdits(*edits, name, fallbackOnly);
    if (!overrideItem) {
        return LINGLONG_ERR(overrideItem.error());
    }

    nlohmann::json root = nlohmann::json::object();
    std::error_code ec;
    if (std::filesystem::exists(configPath, ec) && !ec) {
        auto loaded = loadJsonFile(configPath);
        if (!loaded) {
            return LINGLONG_ERR(loaded.error());
        }
        root = std::move(*loaded);
        if (!root.is_object()) {
            root = nlohmann::json::object();
        }
    }

    nlohmann::json overrides = nlohmann::json::array();
    auto existingOverrides = root.find("extension_overrides");
    if (existingOverrides != root.end() && existingOverrides->is_array()) {
        overrides = *existingOverrides;
    }

    bool replaced = false;
    for (auto &item : overrides) {
        if (!item.is_object()) {
            continue;
        }
        if (item.value("name", "") == name) {
            item = std::move(*overrideItem);
            replaced = true;
            break;
        }
    }
    if (!replaced) {
        overrides.push_back(std::move(*overrideItem));
    }
    root["extension_overrides"] = std::move(overrides);

    auto configDir = configPath.parent_path();
    if (!configDir.empty()) {
        std::filesystem::create_directories(configDir, ec);
        if (ec) {
            return LINGLONG_ERR("failed to create config directory: " + ec.message());
        }
    }

    std::ofstream out(configPath);
    if (!out.is_open()) {
        return LINGLONG_ERR("failed to write config: " + configPath.string());
    }
    out << root.dump(2);
    return LINGLONG_OK;
}

} // namespace linglong::cli::extension_override
