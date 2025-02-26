/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/runtime/container.h"

#include "linglong/utils/finally/finally.h"
#include "ocppi/runtime/RunOption.hpp"
#include "ocppi/runtime/config/types/Generators.hpp"

#include <QDir>
#include <QStandardPaths>

#include <fstream>
#include <iostream>
#include <utility>

#include <sys/stat.h>
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
        dst.env = src.env;
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
                     QString appID,
                     QString conatinerID,
                     ocppi::cli::CLI &cli)
    : cfg(std::move(cfg))
    , id(std::move(conatinerID))
    , appID(std::move(appID))
    , cli(cli)
{
    Q_ASSERT(cfg.process.has_value());
}

utils::error::Result<void> Container::run(const ocppi::runtime::config::types::Process &process,
                                          ocppi::runtime::RunOption &opt) noexcept
{
    LINGLONG_TRACE(QString("run container %1").arg(this->id));

    std::error_code ec;
    std::filesystem::path runtimeDir =
      QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation).toStdString();

    // bundle dir is already created in ContainerBuilder::create
    auto bundle = runtimeDir / "linglong" / this->id.toStdString();
    if (!std::filesystem::create_directories(bundle, ec) && ec) {
        return LINGLONG_ERR("make rootfs directory", ec);
    }
#ifdef LINGLONG_FONT_CACHE_GENERATOR
    if (!bundle.mkpath("conf.d")) {
        return LINGLONG_ERR("make conf.d directory");
    }
#endif
    auto _ = // NOLINT
      utils::finally::finally([&]() {
          std::error_code ec;
          while (!qgetenv("LINGLONG_DEBUG").isEmpty()) {
              if (!std::filesystem::create_directories(runtimeDir / "linglong/debug", ec) && ec) {
                  qCritical() << "failed to create debug directory:" << ec.message().c_str();
                  break;
              }

              auto archive = runtimeDir / "linglong/debug" / this->id.toStdString();
              std::filesystem::rename(bundle, archive, ec);
              if (ec) {
                  qCritical() << "failed to rename bundle directory:" << ec.message().c_str();
                  break;
              }

              return;
          }

          if (std::filesystem::remove_all(bundle, ec) == static_cast<std::uintmax_t>(-1)) {
              qCritical() << "failed to remove " << bundle.c_str() << ": " << ec.message().c_str();
          }
      });

    auto curProcess =
      std::move(this->cfg.process).value_or(ocppi::runtime::config::types::Process{});
    mergeProcessConfig(curProcess, process);
    this->cfg.process = std::move(curProcess);

    if (this->cfg.process->cwd.empty()) {
        qDebug() << "cwd of process is empty, run process in current directory.";
        this->cfg.process->cwd = ("/run/host/rootfs" + QDir::currentPath()).toStdString();
    }

    if (!this->cfg.process->user) {
        this->cfg.process->user =
          ocppi::runtime::config::types::User{ .gid = ::getgid(), .uid = ::getuid() };
    }

    if (isatty(fileno(stdin)) != 0) {
        this->cfg.process->terminal = true;
    }

    // 在原始args前面添加bash --login -c，这样可以使用/etc/profile配置的环境变量
    if (process.args) {
        const auto &args = process.args.value();
        QStringList bashArgs;
        std::transform(args.begin(),
                       args.end(),
                       std::back_inserter(bashArgs),
                       [](const std::string &arg) {
                           return QString::fromStdString(arg);
                       });

        // 为避免原始args包含空格，每个arg都使用单引号包裹，并对arg内部的单引号进行转义替换
        std::for_each(bashArgs.begin(), bashArgs.end(), [](QString &arg) {
            arg.replace('\'', "'\\''");
            arg.prepend('\'');
            arg.push_back('\'');
        });

        // quickfix: 某些应用在以bash -c启动后，收到SIGTERM后不会完全退出
        bashArgs.push_back("; wait");
        auto arguments = std::vector<std::string>{
            "/bin/bash", "--login", "-e", "-c", bashArgs.join(" ").toStdString(),
        };

        this->cfg.process->args = std::move(arguments);
    } else {
        this->cfg.process->args = { "/bin/bash", "--login" };
    }

#ifdef LINGLONG_FONT_CACHE_GENERATOR
    {
        std::ofstream ofs(bundle.absoluteFilePath("conf.d/99-linglong.conf").toStdString());
        Q_ASSERT(ofs.is_open());
        if (!ofs.is_open()) {
            return LINGLONG_ERR("create font config in bundle directory");
        }
        ofs << "<?xml version=\"1.0\"?>" << std::endl;
        ofs << "<!DOCTYPE fontconfig SYSTEM \"urn:fontconfig:fonts.dtd\">" << std::endl;
        ofs << "<fontconfig>" << std::endl;
        ofs << " <include ignore_missing=\"yes\">/run/linglong/cache/fonts/fonts.conf</include>"
            << std::endl;
        ofs << "</fontconfig>" << std::endl;
    }

    this->cfg.mounts->push_back(ocppi::runtime::config::types::Mount{
      .destination = "/etc/fonts/conf.d",
      .gidMappings = {},
      .options = { { "ro", "rbind" } },
      .source = bundle.absoluteFilePath("conf.d").toStdString(),
      .type = "bind",
      .uidMappings = {},
    });
#endif

    {
        std::ofstream ofs(bundle / "config.json");
        Q_ASSERT(ofs.is_open());
        if (!ofs.is_open()) {
            return LINGLONG_ERR("create config.json in bundle directory");
        }

        ofs << nlohmann::json(this->cfg);
        ofs.close();
    }
    qDebug() << "run container in " << bundle.c_str();
    // 禁用crun自己创建cgroup，便于AM识别和管理玲珑应用
    opt.GlobalOption::extra.emplace_back("--cgroup-manager=disabled");

    auto result = this->cli.run(this->id.toStdString(), bundle, opt);

    if (!result) {
        return LINGLONG_ERR("cli run", result);
    }

    return LINGLONG_OK;
}

} // namespace linglong::runtime
