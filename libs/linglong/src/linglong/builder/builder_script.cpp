/*
 * SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "builder_script.h"

#include "configure.h"
#include "linglong/utils/log/log.h"

#include <system_error>

namespace linglong::builder {

namespace detail {

auto findBuilderFileForExecutable(const std::filesystem::path &executable,
                                  const std::filesystem::path &buildRelative,
                                  const std::filesystem::path &installed,
                                  const std::filesystem::path &installedBinDir) noexcept
  -> utils::error::Result<std::filesystem::path>
{
    LINGLONG_TRACE("find builder file for executable");
    std::error_code ec;
    if (!installedBinDir.empty()
        && (executable.parent_path() == installedBinDir
            || std::filesystem::equivalent(executable.parent_path(), installedBinDir, ec))) {
        ec.clear();
        if (std::filesystem::is_regular_file(installed, ec)) {
            return installed;
        }
        return LINGLONG_ERR("builder file not found: " + installed.string());
    }

    for (auto directory = executable.parent_path(); !directory.empty();
         directory = directory.parent_path()) {
        const auto file = directory / buildRelative;
        ec.clear();
        if (std::filesystem::is_regular_file(file, ec)) {
            return file;
        }
        if (directory == directory.parent_path()) {
            break;
        }
    }
    return LINGLONG_ERR("builder file not found near executable: " + executable.string());
}

} // namespace detail

namespace {

auto runningExecutable() noexcept -> utils::error::Result<std::filesystem::path>
{
    LINGLONG_TRACE("locate running executable");
    std::error_code ec;
    const auto executable = std::filesystem::read_symlink("/proc/self/exe", ec);
    if (ec) {
        return LINGLONG_ERR("failed to locate running executable: " + ec.message());
    }
    return executable;
}

} // namespace

auto findBuilderScript(const std::string &scriptName) noexcept
  -> utils::error::Result<std::filesystem::path>
{
    LINGLONG_TRACE("find builder script");
    if (scriptName.empty() || std::filesystem::path(scriptName).filename() != scriptName) {
        return LINGLONG_ERR("invalid builder script name: " + scriptName);
    }
    auto executable = runningExecutable();
    if (!executable) {
        return LINGLONG_ERR(executable);
    }
    return detail::findBuilderFileForExecutable(
      *executable,
      std::filesystem::path("misc/libexec/linglong") / scriptName,
      std::filesystem::path(LINGLONG_LIBEXEC_DIR) / scriptName,
      BINDIR);
}

auto findBuilderTemplate() noexcept -> utils::error::Result<std::filesystem::path>
{
    LINGLONG_TRACE("find builder template");
    auto executable = runningExecutable();
    if (!executable) {
        return LINGLONG_ERR(executable);
    }
    return detail::findBuilderFileForExecutable(
      *executable,
      "misc/share/linglong/builder/templates/example.yaml",
      std::filesystem::path(LINGLONG_DATA_DIR) / "builder/templates/example.yaml",
      BINDIR);
}

} // namespace linglong::builder
