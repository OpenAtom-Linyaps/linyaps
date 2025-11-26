/* SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.  + *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "run_context.h"
#include "configure.h"

#include "linglong/api/types/v1/Generators.hpp"

#include "linglong/common/display.h"
#include "linglong/extension/extension.h"
#include "linglong/runtime/container_builder.h"
#include "linglong/utils/log/log.h"

#include <utility>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <nlohmann/json.hpp>
#include <optional>
#include <sstream>
#include <unordered_set>
#include <vector>

namespace linglong::runtime {

static std::vector<std::filesystem::path> configBasesUserFirst()
{
    namespace fs = std::filesystem;
    std::vector<fs::path> bases;
    std::unordered_set<std::string> seen;

    auto addBase = [&](const fs::path &candidate) {
        if (candidate.empty()) {
            return;
        }
        auto normalized = candidate.lexically_normal();
        auto key = normalized.string();
        if (key.empty()) {
            return;
        }
        if (seen.insert(key).second) {
            bases.emplace_back(std::move(normalized));
        }
    };

    if (const char *xdgConfigHome = ::getenv("XDG_CONFIG_HOME");
        xdgConfigHome && xdgConfigHome[0] != '\0') {
        addBase(fs::path(xdgConfigHome) / "linglong");
    } else if (const char *homeEnv = ::getenv("HOME"); homeEnv && homeEnv[0] != '\0') {
        addBase(fs::path(homeEnv) / ".config" / "linglong");
    }

    addBase(fs::path(LINGLONG_DATA_DIR) / "config");
    return bases;
}

static std::vector<std::filesystem::path> configBasesFallbackFirst()
{
    auto bases = configBasesUserFirst();
    if (bases.size() <= 1) {
        return bases;
    }
    std::rotate(bases.begin(), std::prev(bases.end()), bases.end());
    return bases;
}

static std::vector<std::string> loadExtensionsFromConfig(const std::string &appId)
{
    namespace fs = std::filesystem;
    std::vector<std::string> result;
    
    // 2. 定义解析函数：从指定 JSON 文件提取 "extensions" 数组，并加入 result
    auto parseExtensions = [&](const fs::path &p) {
        try {
            if (!fs::exists(p)) return;
            std::ifstream in(p);
            if (!in.is_open()) return;
            nlohmann::json j;
            in >> j;
            if (!j.contains("extensions") || !j.at("extensions").is_array()) return;
            for (const auto &elem : j.at("extensions")) {
                if (elem.is_string()) {
                    std::string ext = elem.get<std::string>();
                    if (std::find(result.begin(), result.end(), ext) == result.end()) {
                        result.emplace_back(std::move(ext));
                    }
                }
            }
        } catch (...) {
            // 文件不存在或解析失败时忽略
        }
    };
    
    for (const auto &base : configBasesUserFirst()) {
        parseExtensions(base / "config.json");
        if (!appId.empty()) {
            parseExtensions(base / "apps" / appId / "config.json");
        }
    }
    
    return result;
}

static std::vector<std::string> loadExtensionsFromBase(const std::string &baseId)
{
    namespace fs = std::filesystem;
    std::vector<std::string> result;

    if (baseId.empty()) {
        return result;
    }

    for (const auto &root : configBasesUserFirst()) {
        fs::path cfgPath = root / "base" / baseId / "config.json";
        try {
            if (!fs::exists(cfgPath)) {
                continue;
            }
            std::ifstream in(cfgPath);
            if (!in.is_open()) {
                continue;
            }
            nlohmann::json j;
            in >> j;
            if (!j.contains("extensions") || !j.at("extensions").is_array()) {
                continue;
            }
            for (const auto &elem : j.at("extensions")) {
                if (elem.is_string()) {
                    std::string ext = elem.get<std::string>();
                    if (std::find(result.begin(), result.end(), ext) == result.end()) {
                        result.emplace_back(std::move(ext));
                    }
                }
            }
        } catch (...) {
            // ignore parse errors
        }
    }
    return result;
}

// ===== begin: config helpers for env/mount/commands (Global->Base->App merge) =====
using json = nlohmann::json;

static json loadMergedJsonWithBase(const std::string &appId, const std::string &baseId)
{
    namespace fs = std::filesystem;
    json merged = json::object();

    auto readIfExists = [](const fs::path &p) -> std::optional<json> {
        try {
            if (!fs::exists(p)) {
                return std::nullopt;
            }
            std::ifstream in(p);
            if (!in.is_open()) {
                return std::nullopt;
            }
            json j;
            in >> j;
            return j;
        } catch (...) {
            return std::nullopt;
        }
    };

    for (const auto &root : configBasesFallbackFirst()) {
        if (auto g = readIfExists(root / "config.json")) {
            merged.merge_patch(*g);
        }
        if (!baseId.empty()) {
            if (auto b = readIfExists(root / "base" / baseId / "config.json")) {
                merged.merge_patch(*b);
            }
        }
        if (!appId.empty()) {
            if (auto a = readIfExists(root / "apps" / appId / "config.json")) {
                merged.merge_patch(*a);
            }
        }
    }
    return merged;
}

static std::string expandUserHome(const std::string &path)
{
    if (path == "~" || path.rfind("~/", 0) == 0) {
        const char *home = ::getenv("HOME");
        if (home && home[0]) {
            return path == "~" ? std::string(home) : (std::string(home) + path.substr(1));
        }
    }
    return path;
}

static void collectEnvFromJson(const json &j, std::vector<std::string> &out)
{
    if (!j.contains("env") || !j.at("env").is_object()) {
        return;
    }
    for (auto it = j.at("env").begin(); it != j.at("env").end(); ++it) {
        const std::string key = it.key();
        std::string val = it.value().is_string() ? it.value().get<std::string>() : std::string();
        if (val.find('$') != std::string::npos) {
            qWarning() << "ignore env with variable expansion:" << QString::fromStdString(key);
            continue;
        }
        if (!key.empty() && key.back() == '+') {
            out.emplace_back(key.substr(0, key.size() - 1) + "+=" + val);
        } else {
            out.emplace_back(key + "=" + val);
        }
    }
}

static std::vector<ocppi::runtime::config::types::Mount>
parseFilesystemMounts(const std::string &appId, const json &arr)
{
    using Mount = ocppi::runtime::config::types::Mount;
    std::vector<Mount> mounts;
    if (!arr.is_array()) {
        return mounts;
    }
    for (const auto &e : arr) {
        if (!e.is_object()) {
            continue;
        }
        std::string host = e.value("host", "");
        std::string target = e.value("target", "");
        std::string mode = e.value("mode", "ro");
        bool persist = e.value("persist", false);

        if (host.empty() || target.empty()) {
            continue;
        }
        if (host.find('$') != std::string::npos || target.find('$') != std::string::npos) {
            qWarning() << "ignore mount with variable expansion:" << QString::fromStdString(host)
                       << "->" << QString::fromStdString(target);
            continue;
        }

        host = expandUserHome(host);
        if (persist) {
            const char *home = ::getenv("HOME");
            if (home && home[0] && !appId.empty()) {
                std::filesystem::path p(home);
                p /= ".var/app";
                p /= appId;
                p /= std::filesystem::path(host).filename();
                host = p.string();
            }

            std::error_code ec;
            std::filesystem::path hostPath(host);
            if (!std::filesystem::exists(hostPath, ec)) {
                ec.clear();
                if (!std::filesystem::create_directories(hostPath, ec) && ec) {
                    ec.clear();
                    auto parent = hostPath.parent_path();
                    if (!parent.empty()) {
                        std::filesystem::create_directories(parent, ec);
                    }
                }
            }
            if (ec || !std::filesystem::exists(hostPath, ec)) {
                qWarning() << "failed to prepare persist directory for"
                           << QString::fromStdString(host) << ":" << ec.message().c_str();
                continue;
            }
        }

        Mount m;
        m.type = "bind";
        m.source = host;
        m.destination = target;
        m.options = { { (mode == "rw" ? "rw" : "ro"), "rbind" } };
        mounts.emplace_back(std::move(m));
    }

    return mounts;
}

static void collectMountsFromJson(const std::string &appId,
                                  const json &j,
                                  std::vector<ocppi::runtime::config::types::Mount> &out)
{
    if (!j.contains("filesystem") || !j.at("filesystem").is_array()) {
        return;
    }
    auto mounts = parseFilesystemMounts(appId, j.at("filesystem"));
    std::move(mounts.begin(), mounts.end(), std::back_inserter(out));
}

struct CommandSettings {
    std::vector<std::string> envKVs;
    std::vector<ocppi::runtime::config::types::Mount> mounts;
    std::vector<std::string> argsPrefix;
    std::vector<std::string> argsSuffix;
    std::optional<std::string> entrypoint;
    std::optional<std::string> cwd;
};

static const json *pickCommandNode(const json &merged, const std::string &execName)
{
    if (!merged.contains("commands") || !merged.at("commands").is_object()) {
        return nullptr;
    }
    const auto &cmds = merged.at("commands");
    if (auto it = cmds.find(execName); it != cmds.end() && it->is_object()) {
        return &(*it);
    }
    if (auto it = cmds.find("*"); it != cmds.end() && it->is_object()) {
        return &(*it);
    }
    return nullptr;
}

static void loadStrVec(const json &node, const char *key, std::vector<std::string> &out)
{
    if (!node.contains(key) || !node.at(key).is_array()) {
        return;
    }
    for (const auto &v : node.at(key)) {
        if (v.is_string()) {
            out.emplace_back(v.get<std::string>());
        }
    }
}

static CommandSettings parseCommandSettings(const std::string &appId, const json &node)
{
    CommandSettings cs;
    if (node.contains("env") && node.at("env").is_object()) {
        for (auto it = node.at("env").begin(); it != node.at("env").end(); ++it) {
            const std::string key = it.key();
            std::string val = it.value().is_string() ? it.value().get<std::string>() : std::string();
            if (val.find('$') != std::string::npos) {
                qWarning() << "ignore env with variable expansion in command settings:"
                           << QString::fromStdString(key);
                continue;
            }
            if (!key.empty() && key.back() == '+') {
                cs.envKVs.emplace_back(key.substr(0, key.size() - 1) + "+=" + val);
            } else {
                cs.envKVs.emplace_back(key + "=" + val);
            }
        }
    }
    if (node.contains("filesystem") && node.at("filesystem").is_array()) {
        collectMountsFromJson(appId, node, cs.mounts);
    }
    loadStrVec(node, "args_prefix", cs.argsPrefix);
    loadStrVec(node, "args_suffix", cs.argsSuffix);
    if (node.contains("entrypoint") && node.at("entrypoint").is_string()) {
        cs.entrypoint = node.at("entrypoint").get<std::string>();
    }
    if (node.contains("cwd") && node.at("cwd").is_string()) {
        cs.cwd = node.at("cwd").get<std::string>();
    }
    return cs;
}
// ===== end: config helpers =====


RuntimeLayer::RuntimeLayer(package::Reference ref, RunContext &context)
    : reference(std::move(ref))
    , runContext(context)
    , temporary(false)
{
}

RuntimeLayer::~RuntimeLayer()
{
    if (temporary && layerDir) {
        layerDir->removeRecursively();
    }
}

utils::error::Result<void> RuntimeLayer::resolveLayer(const std::vector<std::string> &modules,
                                                      const std::optional<std::string> &subRef)
{
    LINGLONG_TRACE("resolve layer");

    auto &repo = runContext.get().getRepo();
    utils::error::Result<package::LayerDir> layer(LINGLONG_ERR("null"));
    if (modules.empty()) {
        layer = repo.getMergedModuleDir(reference);
    } else if (modules.size() > 1) {
        layer = repo.getMergedModuleDir(reference, modules);
        temporary = true;
    } else {
        layer = repo.getLayerDir(reference, modules[0], subRef);
    }

    if (!layer) {
        return LINGLONG_ERR("layer doesn't exist: " + reference.toString(), layer);
    }

    layerDir = *layer;
    return LINGLONG_OK;
}

utils::error::Result<api::types::v1::RepositoryCacheLayersItem> RuntimeLayer::getCachedItem()
{
    LINGLONG_TRACE("get cached item");

    if (!cachedItem) {
        auto &repo = runContext.get().getRepo();
        auto item = repo.getLayerItem(reference);
        if (!item) {
            return LINGLONG_ERR("no cached item found: " + reference.toString());
        }
        cachedItem = std::move(item).value();
    }

    return *cachedItem;
}

RunContext::~RunContext()
{
    if (!bundle.empty()) {
        std::error_code ec;
        if (std::filesystem::remove_all(bundle, ec) == static_cast<std::uintmax_t>(-1)) {
            qWarning() << "failed to remove " << bundle.c_str() << ": " << ec.message().c_str();
        }
    }
}

utils::error::Result<void> RunContext::resolve(const linglong::package::Reference &runnable,
                                               const ResolveOptions &options)
{
    LINGLONG_TRACE("resolve RunContext from runnable " + runnable.toString());

    filesystemPolicyCache.reset();

    containerID = runtime::genContainerID(runnable);

    auto item = repo.getLayerItem(runnable);
    if (!item) {
        return LINGLONG_ERR("no cached item found: " + runnable.toString());
    }
    const auto &info = item->info;

    // only base need resolved
    if (info.kind == "base") {
        baseLayer = RuntimeLayer(runnable, *this);
        return resolveExtension(*baseLayer);
    }

    if (info.kind == "app") {
        appLayer = RuntimeLayer(runnable, *this);
        auto runtime = options.runtimeRef.value_or(info.runtime.value_or(""));
        if (!runtime.empty()) {
            auto runtimeFuzzyRef = package::FuzzyReference::parse(runtime);
            if (!runtimeFuzzyRef) {
                return LINGLONG_ERR(runtimeFuzzyRef);
            }

            auto ref = repo.clearReference(*runtimeFuzzyRef,
                                           {
                                             .forceRemote = false,
                                             .fallbackToRemote = false,
                                             .semanticMatching = true,
                                           });
            if (!ref) {
                return LINGLONG_ERR("ref doesn't exist " + runtimeFuzzyRef->toString());
            }
            runtimeLayer = RuntimeLayer(std::move(ref).value(), *this);
        }
    } else if (info.kind == "runtime") {
        runtimeLayer = RuntimeLayer(runnable, *this);
    } else {
        return LINGLONG_ERR("kind " + QString::fromStdString(info.kind) + " is not runnable");
    }

    // all kinds of package has base
    auto baseRef = options.baseRef.value_or(info.base);
    auto baseFuzzyRef = package::FuzzyReference::parse(baseRef);
    if (!baseFuzzyRef) {
        return LINGLONG_ERR(baseFuzzyRef);
    }

    auto ref = repo.clearReference(*baseFuzzyRef,
                                   {
                                     .forceRemote = false,
                                     .fallbackToRemote = false,
                                     .semanticMatching = true,
                                   });
    if (!ref) {
        return LINGLONG_ERR(ref);
    }
    baseLayer = RuntimeLayer(std::move(ref).value(), *this);

    // resolve runtime extension
    if (runtimeLayer) {
        auto ret = resolveExtension(*runtimeLayer);
        if (!ret) {
            return LINGLONG_ERR(ret);
        }
    }

    // resolve base extension
    auto ret = resolveExtension(*baseLayer);
    if (!ret) {
        return LINGLONG_ERR(ret);
    }

    // 手动解析多个扩展
    // 先从命令行选项或配置文件获取扩展列表
    // 先从命令行选项或应用/全局配置获取扩展列表
    std::vector<std::string> extRefs;
    if (options.extensionRefs && !options.extensionRefs->empty()) {
        extRefs = *options.extensionRefs;
    } else {
        extRefs = loadExtensionsFromConfig(runnable.id);
    }

    if (extRefs.empty() && info.cliConfig && info.cliConfig->extensions) {
        extRefs = *info.cliConfig->extensions;
    }

    // 如果未获取到扩展列表，则尝试根据 base 层加载
    if (extRefs.empty()) {
        // 获取 baseId
        std::string baseId;

        // 1. 优先使用 ResolveOptions::baseRef（如果提供）
        if (options.baseRef && !options.baseRef->empty()) {
            // 假设存在 FuzzyReference::parse，可解析出 id 部分
            auto baseRef = linglong::package::FuzzyReference::parse(*options.baseRef);
            if (baseRef) {
                baseId = baseRef->id;
            }
        }

        // 2. 否则从当前运行包信息中获取
        if (baseId.empty()) {
            auto item = repo.getLayerItem(runnable);
            if (item && !item->info.base.empty()) {
                baseId = item->info.base;
            }
        }

        // 3. 若 baseId 非空，则读取 base 配置
        if (!baseId.empty()) {
            extRefs = loadExtensionsFromBase(baseId);
        }
    }

    // 若 extRefs 非空，继续使用原有的手动解析逻辑
    if (!extRefs.empty()) {
        auto manualExtensionDef = makeManualExtensionDefine(extRefs);
        if (!manualExtensionDef) {
            return LINGLONG_ERR(manualExtensionDef);
        }

        auto ret = resolveExtension(*manualExtensionDef);
        if (!ret) {
            return LINGLONG_ERR(ret);
        }
    }

    // all reference are cleard , we can get actual layer directory now
    return resolveLayer(options.depsBinaryOnly,
                        options.appModules.value_or(std::vector<std::string>{}));
}

utils::error::Result<void> RunContext::resolve(const api::types::v1::BuilderProject &target,
                                               const std::filesystem::path &buildOutput)
{
    LINGLONG_TRACE("resolve RunContext from builder project " + target.package.id);

    filesystemPolicyCache.reset();

    auto targetRef = package::Reference::fromBuilderProject(target);
    if (!targetRef) {
        return LINGLONG_ERR(targetRef);
    }
    containerID = runtime::genContainerID(*targetRef);
    targetId = target.package.id;

    if (target.package.kind == "extension") {
        extensionOutput = buildOutput;
    } else if (target.package.kind == "app") {
        appOutput = buildOutput;
    } else if (target.package.kind == "runtime") {
        runtimeOutput = buildOutput;
    } else {
        return LINGLONG_ERR("can't resolve run context from package kind "
                            + QString::fromStdString(target.package.kind));
    }

    auto baseFuzzyRef = package::FuzzyReference::parse(target.base);
    if (!baseFuzzyRef) {
        return LINGLONG_ERR(baseFuzzyRef);
    }

    auto ref = repo.clearReference(*baseFuzzyRef,
                                   {
                                     .forceRemote = false,
                                     .fallbackToRemote = false,
                                     .semanticMatching = true,
                                   });
    if (!ref) {
        return LINGLONG_ERR(ref);
    }
    baseLayer = RuntimeLayer(std::move(ref).value(), *this);

    if (target.runtime) {
        auto runtimeFuzzyRef = package::FuzzyReference::parse(*target.runtime);
        if (!runtimeFuzzyRef) {
            return LINGLONG_ERR(runtimeFuzzyRef);
        }

        ref = repo.clearReference(*runtimeFuzzyRef,
                                  {
                                    .forceRemote = false,
                                    .fallbackToRemote = false,
                                    .semanticMatching = true,
                                  });
        if (!ref) {
            return LINGLONG_ERR("ref doesn't exist " + runtimeFuzzyRef->toString());
        }
        runtimeLayer = RuntimeLayer(std::move(ref).value(), *this);

        auto layer = runtimeLayer->getCachedItem();
        if (!layer) {
            return LINGLONG_ERR("no cached item found: " + runtimeLayer->getReference().toString());
        }

        auto fuzzyRef = package::FuzzyReference::parse(layer->info.base);
        if (!fuzzyRef) {
            return LINGLONG_ERR(fuzzyRef);
        }
        auto ref = repo.clearReference(*fuzzyRef,
                                       {
                                         .forceRemote = false,
                                         .fallbackToRemote = false,
                                         .semanticMatching = true,
                                       });
        if (!ref || *ref != baseLayer->getReference()) {
            auto msg = fmt::format("Base is not compatible with runtime. \n - Current base: {}\n - "
                                   "Current runtime: {}\n - Base required by runtime: {}",
                                   baseLayer->getReference().toString(),
                                   runtimeLayer->getReference().toString(),
                                   layer->info.base);
            return LINGLONG_ERR(msg);
        }
    }

    return resolveLayer(false, {});
}

utils::error::Result<void> RunContext::resolveLayer(bool depsBinaryOnly,
                                                    const std::vector<std::string> &appModules)
{
    LINGLONG_TRACE("resolve layers");

    std::optional<std::string> subRef;
    if (appLayer) {
        auto item = appLayer->getCachedItem();
        if (item && item->info.uuid) {
            subRef = item->info.uuid;
        }
    }

    std::vector<std::string> depsModules;
    if (depsBinaryOnly) {
        depsModules.emplace_back("binary");
    }
    auto ref = baseLayer->resolveLayer(depsModules, subRef);
    if (!ref.has_value()) {
        return LINGLONG_ERR("failed to resolve base layer", ref);
    }

    if (appLayer) {
        auto ref = appLayer->resolveLayer(appModules);
        if (!ref.has_value()) {
            return LINGLONG_ERR("failed to resolve app layer", ref);
        }
    }

    if (runtimeLayer) {
        auto ref = runtimeLayer->resolveLayer(depsModules, subRef);
        if (!ref.has_value()) {
            return LINGLONG_ERR("failed to resolve runtime layer", ref);
        }
    }

    for (auto &ext : extensionLayers) {
        if (!ext.resolveLayer()) {
            qWarning() << "ignore failed extension layer";
            continue;
        }

        auto extensionOf = ext.getExtensionInfo();
        if (!extensionOf) {
            continue;
        }

        const auto &[extensionDefine, layer] = *extensionOf;
        auto extItem = ext.getCachedItem();
        if (!extItem) {
            continue;
        }
        const auto &extInfo = extItem->info;
        if (!extInfo.extImpl) {
            LogW("no ext_impl found for {}", ext.getReference().toString());
            continue;
        }
        const auto &extImpl = *extInfo.extImpl;
        if (!extImpl.env) {
            continue;
        }
        for (const auto &env : *extImpl.env) {
            // if allowEnv is not defined, all envs are allowed
            std::string defaultValue;
            if (extensionDefine.allowEnv) {
                const auto &allowEnv = *extensionDefine.allowEnv;
                auto allowed = allowEnv.find(env.first);
                if (allowed == allowEnv.end()) {
                    LogW("env {} not allowed in {}", env.first, ext.getReference().toString());
                    continue;
                }
                defaultValue = allowed->second;
            }

            std::string res =
              common::strings::replaceSubstring(env.second,
                                                "$PREFIX",
                                                "/opt/extensions/" + ext.getReference().id);
            auto &value = environment[env.first];
            if (value.empty()) {
                value = defaultValue;
            }
            // If $ORIGIN is unset and the default value is empty, the environment variable
            // may become ":NEW_VALUE" or "NEW_VALUE:". We cannot remove the leading/trailing
            // colon because the value might represent a non-path element (e.g., a delimiter)
            res = common::strings::replaceSubstring(res, "$ORIGIN", value);

            value = res;
            LogD("environment[{}]={}", env.first, res);
        }
    }

    return LINGLONG_OK;
}

utils::error::Result<void> RunContext::resolveExtension(RuntimeLayer &layer)
{
    LINGLONG_TRACE("resolve RuntimeLayer extension");

    auto item = layer.getCachedItem();
    if (!item) {
        return LINGLONG_ERR("no cached item found: " + layer.getReference().toString());
    }

    const auto &info = item->info;
    if (info.extensions) {
        return resolveExtension(*info.extensions, info.channel, true);
    }

    return LINGLONG_OK;
}

utils::error::Result<void>
RunContext::resolveExtension(const std::vector<api::types::v1::ExtensionDefine> &extDefs,
                             std::optional<std::string> channel,
                             bool skipOnNotFound)
{
    LINGLONG_TRACE("resolve extension define");

    for (const auto &extDef : extDefs) {
        LogD("handle extensions: {}", extDef.name);
        LogD("version: {}", extDef.version);
        LogD("directory: {}", extDef.directory);
        if (extDef.allowEnv) {
            for (const auto &allowEnv : *extDef.allowEnv) {
                LogD("allowEnv: {}:{}", allowEnv.first, allowEnv.second);
            }
        }

        std::string name = extDef.name;
        auto ext = extension::ExtensionFactory::makeExtension(name);
        if (!ext->shouldEnable(name)) {
            continue;
        }

        std::optional<std::string> version;
        if (!extDef.version.empty()) {
            version = extDef.version;
        }
        auto fuzzyRef = package::FuzzyReference::create(channel, name, version, std::nullopt);
        auto ref =
          repo.clearReference(*fuzzyRef, { .fallbackToRemote = false, .semanticMatching = true });
        if (!ref) {
            LogD("extension is not installed: {}", fuzzyRef->toString());
            if (skipOnNotFound) {
                continue;
            }
            return LINGLONG_ERR("extension is not installed", ref);
        }

        RuntimeLayer layer(*ref, *this);
        auto item = layer.getCachedItem();
        if (!item) {
            return LINGLONG_ERR("failed to get layer item", item);
        }
        if (item->info.kind != "extension") {
            return LINGLONG_ERR(fmt::format("{} is not an extension", ref->toString()));
        }

        auto &extensionLayer = extensionLayers.emplace_back(std::move(layer));
        extensionLayer.setExtensionInfo(
          std::make_pair(extDef, std::reference_wrapper<RuntimeLayer>(extensionLayer)));
    }

    return LINGLONG_OK;
}

utils::error::Result<std::vector<api::types::v1::ExtensionDefine>>
RunContext::makeManualExtensionDefine(const std::vector<std::string> &refs)
{
    LINGLONG_TRACE("make extension define");

    std::vector<api::types::v1::ExtensionDefine> extDefs;
    extDefs.reserve(refs.size());
    for (const auto &ref : refs) {
        auto fuzzyRef = package::FuzzyReference::parse(ref);
        if (!fuzzyRef) {
            return LINGLONG_ERR("failed to parse extension ref", fuzzyRef);
        }

        extDefs.emplace_back(api::types::v1::ExtensionDefine{
          .directory = "/opt/extensions/" + fuzzyRef->id,
          .name = fuzzyRef->id,
          .version = fuzzyRef->version.value_or(""),
        });
    }
    return extDefs;
}

void RunContext::detectDisplaySystem(generator::ContainerCfgBuilder &builder) noexcept
{
    while (true) {
        auto *xOrgDisplayEnv = ::getenv("DISPLAY");
        if (xOrgDisplayEnv == nullptr || xOrgDisplayEnv[0] == '\0') {
            LogD("DISPLAY is not set, ignore it");
            break;
        }

        auto xOrgDisplay = common::display::getXOrgDisplay(xOrgDisplayEnv);
        if (!xOrgDisplay) {
            LogW("failed to get XOrg display: {}, ignore it", xOrgDisplay.error());
            break;
        }

        builder.bindXOrgSocket(xOrgDisplay.value());
        break;
    }

    while (true) {
        auto *xOrgAuthFileEnv = ::getenv("XAUTHORITY");
        if (xOrgAuthFileEnv == nullptr || xOrgAuthFileEnv[0] == '\0') {
            LogD("XAUTHORITY is not set, ignore it");
            break;
        }

        auto xOrgAuthFile = common::display::getXOrgAuthFile(xOrgAuthFileEnv);
        if (!xOrgAuthFile) {
            LogW("failed to get XOrg auth file: {}, ignore it", xOrgAuthFile.error());
            break;
        }

        builder.bindXAuthFile(xOrgAuthFile.value());
        break;
    }

    while (true) {
        auto *waylandDisplayEnv = ::getenv("WAYLAND_DISPLAY");
        if (waylandDisplayEnv == nullptr || waylandDisplayEnv[0] == '\0') {
            LogD("WAYLAND_DISPLAY is not set, ignore it");
            break;
        }

        auto waylandDisplay = common::display::getWaylandDisplay(waylandDisplayEnv);
        if (!waylandDisplay) {
            LogW("failed to get Wayland display: {}, ignore it", waylandDisplay.error());
            break;
        }

        builder.bindWaylandSocket(waylandDisplay.value());
        break;
    }
}

utils::error::Result<void>
RunContext::fillContextCfg(linglong::generator::ContainerCfgBuilder &builder)
{
    LINGLONG_TRACE("fill ContainerCfgBuilder with run context");

    builder.setContainerId(containerID);

    if (!baseLayer) {
        return LINGLONG_ERR("run context doesn't resolved");
    }

    auto bundleDir = runtime::makeBundleDir(containerID);
    if (!bundleDir) {
        return LINGLONG_ERR("failed to get bundle dir of " + QString::fromStdString(containerID));
    }
    bundle = *bundleDir;

    builder.setBasePath(baseLayer->getLayerDir()->absoluteFilePath("files").toStdString());

    if (appOutput) {
        builder.setAppPath(*appOutput, false);
    } else {
        if (appLayer) {
            builder.setAppPath(appLayer->getLayerDir()->absoluteFilePath("files").toStdString());
        }
    }

    if (runtimeOutput) {
        builder.setRuntimePath(*runtimeOutput, false);
    } else {
        if (runtimeLayer) {
            builder.setRuntimePath(
              runtimeLayer->getLayerDir()->absoluteFilePath("files").toStdString());
        }
    }

    std::vector<ocppi::runtime::config::types::Mount> extensionMounts{};
    if (extensionOutput) {
        extensionMounts.push_back(ocppi::runtime::config::types::Mount{
          .destination = "/opt/extensions/" + targetId,
          .gidMappings = {},
          .options = { { "rbind" } },
          .source = extensionOutput,
          .type = "bind",
          .uidMappings = {},
        });
    }

    for (const auto &ext : extensionLayers) {
        std::string name = ext.getReference().id;
        if (extensionOutput && name == targetId) {
            continue;
        }
        extensionMounts.push_back(ocppi::runtime::config::types::Mount{
          .destination = "/opt/extensions/" + name,
          .gidMappings = {},
          .options = { { "rbind", "ro" } },
          .source = ext.getLayerDir()->absoluteFilePath("files").toStdString(),
          .type = "bind",
          .uidMappings = {},
        });
    }
    if (!extensionMounts.empty()) {
        builder.setExtensionMounts(extensionMounts);
    }

    builder.setBundlePath(bundle);

    auto res = fillExtraAppMounts(builder);
    if (!res) {
        return res;
    }

    // === begin: merge Global->Base->App config ===
    std::string currentAppId;
    if (appLayer) currentAppId = appLayer->getReference().id;
    else if (!targetId.empty()) currentAppId = targetId;

    std::string currentBaseId;
    if (baseLayer) currentBaseId = baseLayer->getReference().id;

    json mergedCfg = json::object();

    auto mergeCliConfig = [&](RuntimeLayer *layer) {
        if (layer == nullptr) {
            return;
        }
        auto item = layer->getCachedItem();
        if (!item) {
            return;
        }
        if (item->info.cliConfig) {
            json cfg = *(item->info.cliConfig);
            if (mergedCfg.empty()) {
                mergedCfg = std::move(cfg);
            } else {
                mergedCfg.merge_patch(cfg);
            }
        }
    };

    mergeCliConfig(baseLayer ? &*baseLayer : nullptr);
    mergeCliConfig(appLayer ? &*appLayer : nullptr);

    auto configFromDirs = loadMergedJsonWithBase(currentAppId, currentBaseId);
    if (mergedCfg.empty()) {
        mergedCfg = configFromDirs;
    } else {
        mergedCfg.merge_patch(configFromDirs);
    }
    std::optional<std::string> mergedPath;

    // 1) common env
    {
        std::vector<std::string> envKVs;
        collectEnvFromJson(mergedCfg, envKVs);
        if (!envKVs.empty()) {
            std::map<std::string, std::string> genEnv;
            std::string basePath;
            if (auto sysPath = ::getenv("PATH")) {
                basePath = sysPath;
            }
            auto extPathIt = environment.find("PATH");
            for (const auto &kv : envKVs) {
                auto pos = kv.find("+=");
                if (pos != std::string::npos) {
                    auto key = kv.substr(0, pos);
                    auto add = kv.substr(pos + 2);
                    if (key == "PATH") {
                        if (genEnv.count("PATH")) {
                            genEnv["PATH"] += ":" + add;
                        } else if (extPathIt != environment.end()) {
                            genEnv["PATH"] =
                              extPathIt->second.empty() ? add : extPathIt->second + ":" + add;
                        } else if (!basePath.empty()) {
                            genEnv["PATH"] = basePath + ":" + add;
                        } else {
                            genEnv["PATH"] = add;
                        }
                    } else {
                        qWarning() << "ignore '+=' env for key:" << QString::fromStdString(key);
                    }
                    continue;
                }
                auto eq = kv.find('=');
                if (eq == std::string::npos) {
                    continue;
                }
                genEnv[kv.substr(0, eq)] = kv.substr(eq + 1);
            }
            if (!genEnv.empty()) {
                if (auto it = genEnv.find("PATH"); it != genEnv.end()) {
                    mergedPath = it->second;
                }
                builder.appendEnv(genEnv);
            }
        }
    }

    // 2) common filesystem
    {
        const auto &fsPolicy = filesystemPolicy();
        if (fsPolicy.allowListConfigured) {
            if (!fsPolicy.allowList.empty()) {
                auto allowList = fsPolicy.allowList;
                builder.addExtraMounts(std::move(allowList));
            }
        } else if (!fsPolicy.extra.empty()) {
            auto extraMounts = fsPolicy.extra;
            builder.addExtraMounts(std::move(extraMounts));
        }
    }
    // === end: merge Global->Base->App config ===

    if (!environment.empty()) {
        if (auto it = environment.find("PATH"); it != environment.end()) {
            mergedPath = it->second;
        }
        builder.appendEnv(environment);
    }

    // === begin: command-level settings (highest priority) ===
    {
        std::string execName = currentAppId;
        if (!execName.empty()) {
            if (const json *node = pickCommandNode(mergedCfg, execName)) {
                CommandSettings cs = parseCommandSettings(currentAppId, *node);

                if (!cs.envKVs.empty()) {
                    std::map<std::string, std::string> cmdEnv;
                    std::string basePath;
                    if (auto sysPath = ::getenv("PATH")) {
                        basePath = sysPath;
                    }
                    auto extPathIt = environment.find("PATH");
                    for (const auto &kv : cs.envKVs) {
                        auto posp = kv.find("+=");
                        if (posp != std::string::npos) {
                            auto key = kv.substr(0, posp);
                            auto add = kv.substr(posp + 2);
                            if (key == "PATH") {
                                if (cmdEnv.count("PATH")) {
                                    cmdEnv["PATH"] += ":" + add;
                                } else if (mergedPath) {
                                    cmdEnv["PATH"] =
                                      mergedPath->empty() ? add : *mergedPath + ":" + add;
                                } else if (extPathIt != environment.end()) {
                                    cmdEnv["PATH"] =
                                      extPathIt->second.empty()
                                        ? add
                                        : extPathIt->second + ":" + add;
                                } else if (!basePath.empty()) {
                                    cmdEnv["PATH"] = basePath + ":" + add;
                                } else {
                                    cmdEnv["PATH"] = add;
                                }
                            } else {
                                qWarning() << "ignore '+=' env for key in command settings:"
                                           << QString::fromStdString(key);
                            }
                            continue;
                        }
                        auto eq = kv.find('=');
                        if (eq == std::string::npos) {
                            continue;
                        }
                        cmdEnv[kv.substr(0, eq)] = kv.substr(eq + 1);
                    }
                    if (!cmdEnv.empty()) {
                        if (auto it = cmdEnv.find("PATH"); it != cmdEnv.end()) {
                            mergedPath = it->second;
                        }
                        builder.appendEnv(cmdEnv);
                    }
                }

                if (!cs.mounts.empty()) builder.addExtraMounts(cs.mounts);
                // TODO: when builder exposes API for entrypoint/cwd/args, apply here as well.
            }
        }
    }
    // === end: command-level settings ===

    detectDisplaySystem(builder);

    for (auto ctx = securityContexts.begin(); ctx != securityContexts.end(); ++ctx) {
        auto manager = getSecurityContextManager(ctx->first);
        if (!manager) {
            auto msg = "failed to get security context manager: " + fromType(ctx->first);
            return LINGLONG_ERR(msg.c_str());
        }

        auto secCtx = manager->createSecurityContext(builder);
        if (!secCtx) {
            auto msg = "failed to create security context: " + fromType(ctx->first);
            return LINGLONG_ERR(msg.c_str());
        }
        ctx->second = std::move(secCtx).value();

        auto res = ctx->second->apply(builder);
        if (!res) {
            auto msg = "failed to apply security context: " + fromType(ctx->first);
            ctx = securityContexts.erase(ctx);
            return LINGLONG_ERR(msg.c_str(), res);
        }
    }

    return LINGLONG_OK;
}

void RunContext::enableSecurityContext(const std::vector<SecurityContextType> &ctxs)
{
    for (const auto &type : ctxs) {
        securityContexts.try_emplace(type, nullptr);
    }
}

utils::error::Result<void> RunContext::fillExtraAppMounts(generator::ContainerCfgBuilder &builder)
{
    LINGLONG_TRACE("fill extra app mounts");

    auto fillPermissionsBinds = [&builder,
                                 this](RuntimeLayer &layer) -> utils::error::Result<void> {
        LINGLONG_TRACE("fill permissions binds");

        auto item = layer.getCachedItem();
        if (!item) {
            return LINGLONG_ERR(item);
        }
        const auto &info = item->info;

        if (info.permissions) {
            std::vector<ocppi::runtime::config::types::Mount> applicationMounts{};
            auto bindMount =
              [&applicationMounts](
                const api::types::v1::ApplicationConfigurationPermissionsBind &bind) {
                  applicationMounts.push_back(ocppi::runtime::config::types::Mount{
                    .destination = bind.destination,
                    .gidMappings = {},
                    .options = { { "rbind" } },
                    .source = bind.source,
                    .type = "bind",
                    .uidMappings = {},
                  });
              };

            auto bindInnerMount =
              [&applicationMounts,
               this](const api::types::v1::ApplicationConfigurationPermissionsInnerBind &bind) {
                  applicationMounts.push_back(ocppi::runtime::config::types::Mount{
                    .destination = bind.destination,
                    .gidMappings = {},
                    .options = { { "rbind" } },
                    .source = bundle.string() + "/rootfs" + bind.source,
                    .type = "bind",
                    .uidMappings = {},
                  });
              };

            const auto &perm = info.permissions;
            if (perm->binds) {
                const auto &binds = perm->binds;
                std::for_each(binds->cbegin(), binds->cend(), bindMount);
            }

            if (perm->innerBinds) {
                const auto &innerBinds = perm->innerBinds;
                std::for_each(innerBinds->cbegin(), innerBinds->cend(), bindInnerMount);
            }

            builder.addExtraMounts(applicationMounts);
        }

        return LINGLONG_OK;
    };

    if (appLayer) {
        auto res = fillPermissionsBinds(*appLayer);
        if (!res) {
            return LINGLONG_ERR("failed to apply permission binds for "
                                  + appLayer->getReference().toString(),
                                res);
        }
    }

    for (auto &ext : extensionLayers) {
        if (!fillPermissionsBinds(ext)) {
            LogW("failed to apply permission binds for {}", ext.getReference().toString());
            continue;
        }
    }

    return LINGLONG_OK;
}

api::types::v1::ContainerProcessStateInfo RunContext::stateInfo()
{
    auto state = linglong::api::types::v1::ContainerProcessStateInfo{
        .containerID = containerID,
    };

    if (baseLayer) {
        state.base = baseLayer->getReference().toString();
    }

    if (appLayer) {
        state.app = appLayer->getReference().toString();
    }

    if (runtimeLayer) {
        state.runtime = runtimeLayer->getReference().toString();
    }

    state.extensions = std::vector<std::string>{};
    for (auto &ext : extensionLayers) {
        state.extensions->push_back(ext.getReference().toString());
    }

    return state;
}

utils::error::Result<std::filesystem::path> RunContext::getBaseLayerPath() const
{
    LINGLONG_TRACE("get base layer path");

    if (!baseLayer) {
        return LINGLONG_ERR("run context doesn't resolved");
    }

    const auto &layerDir = baseLayer->getLayerDir();
    return std::filesystem::path{ layerDir->absolutePath().toStdString() };
}

std::string RunContext::currentAppId() const
{
    if (appLayer) {
        return appLayer->getReference().id;
    }
    return targetId;
}

const RunContext::FilesystemPolicy &RunContext::filesystemPolicy() const
{
    if (!filesystemPolicyCache) {
        FilesystemPolicy policy;

        auto appId = currentAppId();
        std::string baseId;
        if (baseLayer) {
            baseId = baseLayer->getReference().id;
        }

        auto mergedCfg = loadMergedJsonWithBase(appId, baseId);
        if (auto it = mergedCfg.find("filesystem_allow_only"); it != mergedCfg.end()) {
            policy.allowListConfigured = true;
            if (it->is_array()) {
                policy.allowList = parseFilesystemMounts(appId, *it);
            }
        }

        if (!policy.allowListConfigured) {
            if (auto it = mergedCfg.find("filesystem"); it != mergedCfg.end()) {
                policy.extra = parseFilesystemMounts(appId, *it);
            }
        }

        filesystemPolicyCache = std::move(policy);
    }

    return *filesystemPolicyCache;
}

utils::error::Result<std::filesystem::path> RunContext::getRuntimeLayerPath() const
{
    LINGLONG_TRACE("get runtime layer path");

    if (!runtimeLayer) {
        return LINGLONG_ERR("no runtime layer exist");
    }

    const auto &layerDir = runtimeLayer->getLayerDir();
    return std::filesystem::path{ layerDir->absolutePath().toStdString() };
}

utils::error::Result<api::types::v1::RepositoryCacheLayersItem> RunContext::getCachedAppItem()
{
    LINGLONG_TRACE("get cached app item");

    if (!appLayer) {
        return LINGLONG_ERR("no app layer exist");
    }

    return appLayer->getCachedItem();
}

} // namespace linglong::runtime
