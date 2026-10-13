// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linglong/cli/doctor.h"

#include "configure.h"
#include "linglong/common/dir.h"
#include "linglong/repo/config.h"
#include "linglong/runtime/overlayfs_driver.h"
#include "linglong/utils/cmd.h"

#include <fmt/format.h>
#include <fmt/ranges.h>

#include <QStandardPaths>
#include <QString>

#include <algorithm>
#include <array>
#include <charconv>
#include <filesystem>
#include <fstream>

namespace linglong::cli {

namespace {

std::optional<std::array<long long, 3>> parseVersionTuple(const std::string &version)
{
    // keep the numeric prefix, e.g. "1.15.0-dev+1fc6f59" -> 1.15.0
    auto core = version.substr(0, version.find('-'));

    std::array<long long, 3> numbers{ 0, 0, 0 };
    std::size_t begin = 0;
    for (std::size_t i = 0; i < numbers.size(); ++i) {
        auto end = core.find('.', begin);
        auto part = core.substr(begin, end == std::string::npos ? std::string::npos : end - begin);
        if (part.empty()) {
            return std::nullopt;
        }
        auto value = std::from_chars(part.data(), part.data() + part.size(), numbers[i]);
        if (value.ec != std::errc{} || value.ptr != part.data() + part.size()) {
            return std::nullopt;
        }
        if (end == std::string::npos) {
            break;
        }
        begin = end + 1;
    }
    return numbers;
}

DoctorCheckResult checkOCIRuntime()
{
    DoctorCheckResult result{ .name = "oci-runtime", .required = true, .ok = false, .detail = "" };

    auto ociRuntimeCLI = qgetenv("LINGLONG_OCI_RUNTIME");
    if (ociRuntimeCLI.isEmpty()) {
        ociRuntimeCLI = LINGLONG_DEFAULT_OCI_RUNTIME;
    }

    auto path = QStandardPaths::findExecutable(ociRuntimeCLI);
    if (path.isEmpty()) {
        result.detail = fmt::format("{} not found in PATH, sandbox operations will fail; install "
                                    "it or point LINGLONG_OCI_RUNTIME to a usable runtime",
                                    ociRuntimeCLI.toStdString());
        return result;
    }

    result.ok = true;
    result.detail = fmt::format("found {} at {}", ociRuntimeCLI.toStdString(), path.toStdString());
    return result;
}

DoctorCheckResult checkRepository()
{
    DoctorCheckResult result{ .name = "repository", .required = true, .ok = false, .detail = "" };

    const std::filesystem::path versionFile = std::filesystem::path(LINGLONG_ROOT) / ".version";
    std::error_code ec;
    if (!std::filesystem::exists(versionFile, ec)) {
        result.detail =
          fmt::format("{} not found, no repository has been initialized yet", versionFile.string());
        // a missing repository is initialized on demand, not an error
        result.ok = true;
        result.required = false;
        return result;
    }

    std::ifstream in(versionFile);
    std::string repoVersion((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    while (!repoVersion.empty() && (repoVersion.back() == '\n' || repoVersion.back() == '\r')) {
        repoVersion.pop_back();
    }

    auto cmp = compareVersions(repoVersion, LINGLONG_VERSION);
    if (!cmp) {
        result.detail = fmt::format("cannot parse repository version {} in {}",
                                    repoVersion,
                                    versionFile.string());
        return result;
    }
    if (*cmp > 0) {
        result.detail = fmt::format("repository version {} is newer than this tool ({}), "
                                    "downgrading the repository is unsafe; upgrade linyaps",
                                    repoVersion,
                                    LINGLONG_VERSION);
        return result;
    }

    result.ok = true;
    result.detail = fmt::format("repository at {} is version {}", LINGLONG_ROOT, repoVersion);
    return result;
}

DoctorCheckResult checkOverlaySupport()
{
    DoctorCheckResult result{ .name = "overlayfs", .required = true, .ok = false, .detail = "" };

    auto mode = runtime::OverlayFSDriver::resolveOverlayMode(utils::OverlayMode::Auto);
    if (!mode) {
        result.detail = "neither kernel overlayfs nor a FUSE overlay backend (fuse-overlayfs and "
                        "fusermount) is usable, containers cannot be prepared";
        return result;
    }

    result.ok = true;
    result.detail = fmt::format("using {} overlay backend",
                                std::string(runtime::OverlayFSDriver::modeToString(*mode)));
    return result;
}

DoctorCheckResult checkErofsTools()
{
    DoctorCheckResult result{ .name = "erofs-tools", .required = true, .ok = false, .detail = "" };

    std::vector<std::string> missing;
    for (const auto *tool : { "mkfs.erofs", "fsck.erofs" }) {
        if (!utils::Cmd(tool).exists()) {
            missing.emplace_back(tool);
        }
    }

    if (!missing.empty()) {
        result.detail = fmt::format("missing {}, required to build and install layer/uab packages",
                                    fmt::join(missing, ", "));
        return result;
    }

    result.ok = true;
    result.detail = "mkfs.erofs and fsck.erofs are available";
    return result;
}

DoctorCheckResult checkErofsFuse()
{
    DoctorCheckResult result{ .name = "erofsfuse", .required = false, .ok = true, .detail = "" };

    if (utils::Cmd("erofsfuse").exists()) {
        result.detail = "erofsfuse is available";
        return result;
    }

    result.detail = "erofsfuse not found, uab layers are mounted through fsck.erofs instead";
    return result;
}

DoctorCheckResult checkUserRuntimeDir()
{
    DoctorCheckResult result{ .name = "runtime-dir", .required = true, .ok = false, .detail = "" };

    auto runtimeDir = common::dir::getRuntimeDir();
    std::error_code ec;
    if (!std::filesystem::exists(runtimeDir, ec)) {
        result.detail =
          fmt::format("{} does not exist, it is created on first use", runtimeDir.string());
        // directories below the user runtime are created on demand
        result.ok = true;
        result.required = false;
        return result;
    }
    if (!std::filesystem::is_directory(runtimeDir, ec)) {
        result.detail = fmt::format("{} exists but is not a directory", runtimeDir.string());
        return result;
    }

    auto probe = runtimeDir / ".linglong-doctor-probe";
    if (!std::filesystem::exists(probe, ec)) {
        std::ofstream touch(probe);
        if (!touch.is_open()) {
            result.detail =
              fmt::format("{} is not writable by the current user", runtimeDir.string());
            return result;
        }
        touch.close();
    }
    std::filesystem::remove(probe, ec);

    result.ok = true;
    result.detail = fmt::format("{} is writable", runtimeDir.string());
    return result;
}

} // namespace

std::optional<int> compareVersions(const std::string &lhs, const std::string &rhs)
{
    auto lhsTuple = parseVersionTuple(lhs);
    auto rhsTuple = parseVersionTuple(rhs);
    if (!lhsTuple || !rhsTuple) {
        return std::nullopt;
    }

    for (std::size_t i = 0; i < lhsTuple->size(); ++i) {
        if ((*lhsTuple)[i] != (*rhsTuple)[i]) {
            return (*lhsTuple)[i] < (*rhsTuple)[i] ? -1 : 1;
        }
    }
    return 0;
}

std::vector<DoctorCheckResult> runDoctorChecks()
{
    std::vector<DoctorCheckResult> results;
    results.reserve(6);
    results.push_back(checkOCIRuntime());
    results.push_back(checkRepository());
    results.push_back(checkOverlaySupport());
    results.push_back(checkErofsTools());
    results.push_back(checkErofsFuse());
    results.push_back(checkUserRuntimeDir());
    return results;
}

} // namespace linglong::cli
