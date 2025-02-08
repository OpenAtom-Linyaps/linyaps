/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/runtime/container.h"

#include "linglong/package/architecture.h"
#include "linglong/utils/finally/finally.h"
#include "ocppi/runtime/RunOption.hpp"
#include "ocppi/runtime/config/types/Generators.hpp"

#include <QDir>
#include <QStandardPaths>

#include <filesystem>
#include <fstream>

#include <sys/stat.h>
#include <unistd.h>

namespace linglong::runtime {

Container::Container(const ocppi::runtime::config::types::Config &cfg,
                     const QString &appID,
                     const QString &conatinerID,
                     ocppi::cli::CLI &cli)
    : cfg(cfg)
    , id(conatinerID)
    , appID(appID)
    , cli(cli)
{
    Q_ASSERT(cfg.process.has_value());
}

utils::error::Result<void> Container::run(const ocppi::runtime::config::types::Process &process,
                                          ocppi::runtime::RunOption &opt) noexcept
{
    LINGLONG_TRACE(QString("run container %1").arg(this->id));

    QDir runtimeDir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);

    // bundle dir is already created in ContainerBuilder::create
    QDir bundle = runtimeDir.absoluteFilePath(QString("linglong/%1").arg(this->id));
    if (!bundle.mkpath("./rootfs")) {
        return LINGLONG_ERR("make rootfs directory");
    }
#ifdef LINGLONG_FONT_CACHE_GENERATOR
    if (!bundle.mkpath("conf.d")) {
        return LINGLONG_ERR("make conf.d directory");
    }
#endif
    auto _ = // NOLINT
      utils::finally::finally([&]() {
          if (!qgetenv("LINGLONG_DEBUG").isEmpty()) {
              runtimeDir.mkpath("linglong/debug");
              auto archive =
                runtimeDir.absoluteFilePath(QString("linglong/debug/%1").arg(this->id));
              if (QDir().rename(bundle.absolutePath(), archive)) {
                  return;
              }
              qCritical() << "failed to archive" << bundle.absolutePath() << "to" << archive;
          }
          if (bundle.removeRecursively()) {
              return;
          }
          qCritical() << "failed to remove" << runtimeDir.absolutePath();
      });

    if (!this->cfg.process) {
        // NOTE: process should be set in /usr/lib/linglong/container/config.json,
        // and configuration generator must not delete it.
        // So it's a bug if process no has_value at this time.
        Q_ASSERT(false);
        return LINGLONG_ERR("process is not set");
    }

    if (!this->cfg.process->env.has_value()) {
        // NOTE: same as above.
        Q_ASSERT(false);
        return LINGLONG_ERR("process.env is not set");
    }

    auto originEnvs = this->cfg.process->env.value();
    this->cfg.process = process;

    if (this->cfg.process->user) {
        qWarning() << "`user` field is ignored.";
    }

    if (!this->cfg.process->env) {
        qDebug() << "user `env` field is not exists.";
        this->cfg.process->env = std::vector<std::string>{};
    }

    if (this->cfg.process->cwd.empty()) {
        qDebug() << "cwd of process is empty, run process in current directory.";
        this->cfg.process->cwd = ("/run/host/rootfs" + QDir::currentPath()).toStdString();
    }

    this->cfg.process->user = ocppi::runtime::config::types::User{};
    this->cfg.process->user->gid = getgid();
    this->cfg.process->user->uid = getuid();

    if (isatty(fileno(stdin)) != 0) {
        this->cfg.process->terminal = true;
    }
    // 在原始args前面添加bash --login -c，这样可以使用/etc/profile配置的环境变量
    if (process.args.has_value()) {
        QStringList bashArgs;
        // 为避免原始args包含空格，每个arg都使用单引号包裹，并对arg内部的单引号进行转义替换
        for (const auto &arg : *process.args) {
            bashArgs.push_back(
              QString("'%1'").arg(QString::fromStdString(arg).replace("'", "'\\''")));
        }
        // quickfix: 某些应用在以bash -c启动后，收到SIGTERM后不会完全退出
        bashArgs.push_back("; wait");
        auto arguments = std::vector<std::string>{
            "/bin/bash", "--login", "-e", "-c", bashArgs.join(" ").toStdString(),
        };
        this->cfg.process->args = arguments;
    }

    for (const auto &env : *this->cfg.process->env) {
        auto key = env.substr(0, env.find_first_of('='));
        auto it = // NOLINT
          std::find_if(originEnvs.cbegin(), originEnvs.cend(), [&key](const std::string &env) {
              return env.rfind(key, 0) == 0;
          });

        if (it != originEnvs.cend()) {
            qWarning() << "duplicate environment has been detected: ["
                       << "original:" << QString::fromStdString(*it)
                       << "user:" << QString::fromStdString(env) << "], choose original.";
            continue;
        }

        originEnvs.emplace_back(env);
    }

    this->cfg.process->env = originEnvs;

    auto arch = package::Architecture::currentCPUArchitecture();
    if (!arch) {
        return LINGLONG_ERR(arch);
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

    nlohmann::json json = this->cfg;

    {
        std::ofstream ofs(bundle.absoluteFilePath("config.json").toStdString());
        Q_ASSERT(ofs.is_open());
        if (!ofs.is_open()) {
            return LINGLONG_ERR("create config.json in bundle directory");
        }

        ofs << json.dump();
        ofs.close();
    }
    qDebug() << "run container in " << bundle.path();
    // 禁用crun自己创建cgroup，便于AM识别和管理玲珑应用
    opt.GlobalOption::extra.emplace_back("--cgroup-manager=disabled");
    auto result = this->cli.run(this->id.toStdString(),
                                std::filesystem::path(bundle.absolutePath().toStdString()),
                                opt);

    if (!result) {
        return LINGLONG_ERR("cli run", result);
    }

    return LINGLONG_OK;
}

} // namespace linglong::runtime
