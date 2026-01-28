/*
 * SPDX-FileCopyrightText: 2022 - 2025 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/runtime/container.h"

#include "configure.h"
#include "linglong/common/dir.h"
#include "linglong/utils/bash_command_helper.h"
#include "linglong/utils/file.h"
#include "linglong/utils/finally/finally.h"
#include "linglong/utils/log/log.h"
#include "ocppi/runtime/RunOption.hpp"
#include "ocppi/runtime/config/types/Generators.hpp"

#include <fmt/format.h>

#include <cassert>
#include <fstream>
#include <utility>

#include <unistd.h>

namespace {
void mergeProcessConfig(ocppi::runtime::config::types::Process &dst,
                        const ocppi::runtime::config::types::Process &src)
{
    if (src.user) {
        dst.user = src.user;
    }

    if (src.apparmorProfile) {
        dst.apparmorProfile = src.apparmorProfile;
    }

    if (src.args) {
        dst.args = src.args;
    }

    if (src.capabilities) {
        dst.capabilities = src.capabilities;
    }

    if (src.commandLine) {
        dst.commandLine = src.commandLine;
    }

    if (src.consoleSize) {
        dst.consoleSize = src.consoleSize;
    }

    if (!src.cwd.empty()) {
        dst.cwd = src.cwd;
    }

    if (src.env) {
        if (!dst.env) {
            dst.env = src.env;
        } else {
            auto &dstEnv = dst.env.value();
            for (const auto &env : src.env.value()) {
                auto key = env.find_first_of('=');
                if (key == std::string::npos) {
                    continue;
                }

                auto it =
                  std::find_if(dstEnv.begin(), dstEnv.end(), [&key, &env](const std::string &dst) {
                      return dst.rfind(std::string_view(env.data(), key + 1), 0) == 0;
                  });

                if (it != dstEnv.end()) {
                    LogW("environment set multiple times {} {}", *it, env);
                    *it = env;
                } else {
                    dstEnv.emplace_back(env);
                }
            }
        }
    }

    if (src.ioPriority) {
        dst.ioPriority = src.ioPriority;
    }

    if (src.noNewPrivileges) {
        dst.noNewPrivileges = src.noNewPrivileges;
    }

    if (src.oomScoreAdj) {
        dst.oomScoreAdj = src.oomScoreAdj;
    }

    if (src.rlimits) {
        dst.rlimits = src.rlimits;
    }

    if (src.scheduler) {
        dst.scheduler = src.scheduler;
    }

    if (src.selinuxLabel) {
        dst.selinuxLabel = src.selinuxLabel;
    }

    if (src.terminal) {
        dst.terminal = src.terminal;
    }

    if (src.user) {
        dst.user = src.user;
    }
}
} // namespace

namespace linglong::runtime {

Container::Container(ocppi::runtime::config::types::Config cfg,
                     std::string containerId,
                     std::filesystem::path bundleDir,
                     ocppi::cli::CLI &cli)
    : cfg(std::move(cfg))
    , id(std::move(containerId))
    , bundleDir(std::move(bundleDir))
    , cli(cli)
{
    assert(cfg.process.has_value());
}

utils::error::Result<void> Container::run(const ocppi::runtime::config::types::Process &process,
                                          ocppi::runtime::RunOption &opt) noexcept
{
    LINGLONG_TRACE(fmt::format("run container {}", this->id));

    auto _ = utils::finally::finally([&]() {
        std::error_code ec;
        while (getenv("LINGLONG_DEBUG") != nullptr) {
            std::filesystem::path debugDir = common::dir::getRuntimeDir() / "debug";
            std::filesystem::create_directories(debugDir, ec);
            if (ec) {
                LogE("failed to create debug directory {}: {}", debugDir, ec.message());
                break;
            }

            auto archive = debugDir / this->bundleDir.filename();
            std::filesystem::rename(this->bundleDir, archive, ec);
            if (ec) {
                LogE("failed to rename bundle directory to {}: {}", archive, ec.message());
                break;
            }

            return;
        }

        std::filesystem::remove_all(this->bundleDir, ec);
        if (ec) {
            LogW("failed to remove bundle directory {}: {}", this->bundleDir, ec.message());
        }
    });

    auto curProcess =
      std::move(this->cfg.process).value_or(ocppi::runtime::config::types::Process{});
    mergeProcessConfig(curProcess, process);
    this->cfg.process = std::move(curProcess);

    std::error_code ec;
    if (this->cfg.process->cwd.empty()) {
        auto cwd = std::filesystem::current_path(ec);
        LogD("cwd of process is empty, run process in current directory {}.", cwd);
        this->cfg.process->cwd = std::filesystem::path{ "/run/host/rootfs" } / cwd;
    }

    if (!this->cfg.process->user) {
        this->cfg.process->user =
          ocppi::runtime::config::types::User{ .gid = ::getgid(), .uid = ::getuid() };
    }

    if (isatty(fileno(stdin)) != 0) {
        this->cfg.process->terminal = true;
    }

    this->cfg.mounts->push_back(ocppi::runtime::config::types::Mount{
      .destination = "/run/linglong/container-init",
      .options = { { "ro", "rbind" } },
      .source = std::string{ LINGLONG_CONTAINER_INIT },
      .type = "bind",
    });

    auto originalArgs =
      this->cfg.process->args.value_or(std::vector<std::string>{ "echo", "noting to run" });

    auto entrypoint = bundleDir / "entrypoint.sh";
    auto res = utils::writeFile(entrypoint,
                                utils::BashCommandHelper::generateEntrypointScript(originalArgs));
    if (!res) {
        return LINGLONG_ERR(fmt::format("failed to write to {}", entrypoint), res);
    }

    std::filesystem::permissions(entrypoint, std::filesystem::perms::owner_all, ec);
    if (ec) {
        return LINGLONG_ERR("make entrypoint executable", ec);
    }

    auto entrypointPath = "/run/linglong/entrypoint.sh";

    this->cfg.mounts->push_back(ocppi::runtime::config::types::Mount{
      .destination = entrypointPath,
      .options = { { "ro", "rbind" } },
      .source = entrypoint,
      .type = "bind",
    });

    auto cmd = utils::BashCommandHelper::generateExecCommand(entrypointPath);
    this->cfg.process->args = cmd;
    res = utils::writeFile(bundleDir / "config.json", nlohmann::json(this->cfg).dump());
    if (!res) {
        return LINGLONG_ERR("failed to write to config.json", res);
    }

    LogD("run container with bundle {}", bundleDir);
    // 禁用crun自己创建cgroup，便于AM识别和管理玲珑应用
    opt.GlobalOption::extra.emplace_back("--cgroup-manager=disabled");

    auto result = this->cli.run(this->id, bundleDir, opt);
    if (!result) {
        return LINGLONG_ERR("cli run", result.error());
    }

    return LINGLONG_OK;
}

} // namespace linglong::runtime
