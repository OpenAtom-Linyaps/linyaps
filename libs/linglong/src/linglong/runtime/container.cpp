/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/runtime/container.h"

#include "linglong/utils/configure.h"
#include "linglong/utils/finally/finally.h"
#include "ocppi/runtime/RunOption.hpp"
#include "ocppi/runtime/config/types/Generators.hpp"

#include <QDir>
#include <QStandardPaths>

#include <fstream>
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
                    qWarning() << "environment set multiple times " << QString::fromStdString(*it)
                               << QString::fromStdString(env);
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

    this->cfg.mounts->push_back(ocppi::runtime::config::types::Mount{
      .destination = "/run/linglong/container-init",
      .options = { { "ro", "rbind" } },
      .source = std::string{ LINGLONG_CONTAINER_INIT },
      .type = "bind",
    });

    auto originalArgs =
      this->cfg.process->args.value_or(std::vector<std::string>{ "echo", "'noting to run'" });
    QStringList appArgs;
    std::transform(originalArgs.begin(),
                   originalArgs.end(),
                   std::back_inserter(appArgs),
                   QString::fromStdString);
    appArgs.prepend("exec");

    auto entrypoint = bundle / "entrypoint.sh";
    {
        std::ofstream ofs(entrypoint);
        Q_ASSERT(ofs.is_open());
        if (!ofs.is_open()) {
            return LINGLONG_ERR("create font config in bundle directory");
        }

        // TODO: maybe we could use a symlink '/usr/bin/ll-init' points to
        // '/run/linglong/container-init' will be better
        ofs << "#!/run/linglong/container-init /bin/bash\n";
        ofs << "source /etc/profile\n"; // we need use /etc/profile to generate all needed
                                        // environment variables
        ofs << appArgs.join(' ').toStdString();
    }

    std::filesystem::permissions(entrypoint, std::filesystem::perms::owner_all, ec);
    if (ec) {
        return LINGLONG_ERR("make entrypoint executable", ec);
    }

    this->cfg.mounts->push_back(ocppi::runtime::config::types::Mount{
      .destination = "/run/linglong/entrypoint.sh",
      .options = { { "ro", "rbind" } },
      .source = entrypoint,
      .type = "bind",
    });

    this->cfg.process->args = { "/run/linglong/entrypoint.sh" };

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
