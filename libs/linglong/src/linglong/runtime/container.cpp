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

utils::error::Result<void>
Container::run(const ocppi::runtime::config::types::Process &process) noexcept
{
    LINGLONG_TRACE(QString("run container %1").arg(this->id));

    QDir runtimeDir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    QDir bundle = runtimeDir.absoluteFilePath(QString("linglong/%1").arg(this->id));
    Q_ASSERT(!bundle.exists());
    if (!bundle.mkpath(".")) {
        return LINGLONG_ERR("make bundle directory");
    }
    if (!bundle.mkpath("./rootfs")) {
        return LINGLONG_ERR("make rootfs directory");
    }
    auto _ = utils::finally::finally([&]() {
        if (!qgetenv("LINGLONG_DEBUG").isEmpty()) {
            runtimeDir.mkpath("linglong/debug");
            auto archive = runtimeDir.absoluteFilePath(QString("linglong/debug/%1").arg(this->id));
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

    // we only set args in process parameter for now
    this->cfg.process->args = process.args;

    if (this->cfg.process->user) {
        qWarning() << "`user` field is ignored.";
        Q_ASSERT(false);
    }

    if (!this->cfg.process->env) {
        qWarning() << "`env` field is not exists.";
        Q_ASSERT(false);
        this->cfg.process->env = std::vector<std::string>{};
    }

    if (this->cfg.process->cwd.empty()) {
        qDebug() << "cwd of process is empty, run process in current directory.";
        this->cfg.process->cwd = ("/run/host/rootfs" + QDir::currentPath()).toStdString();
    }

    this->cfg.process->user = ocppi::runtime::config::types::User{
        .gid = getgid(),
        .uid = getuid(),
    };

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

    auto arch = package::Architecture::parse(QSysInfo::currentCpuArchitecture());
    if (!arch) {
        return LINGLONG_ERR(arch);
    }
    {
        std::ofstream ofs(
          bundle.absoluteFilePath("zz_deepin-linglong-app.ld.so.conf").toStdString());
        Q_ASSERT(ofs.is_open());
        if (!ofs.is_open()) {
            return LINGLONG_ERR("create ld config in bundle directory");
        }

        ofs << "/runtime/lib" << std::endl;
        ofs << "/runtime/lib/" + arch->getTriplet().toStdString() << std::endl;
        ofs << "/opt/apps/" + this->appID.toStdString() + "/files/lib" << std::endl;
        ofs << "/opt/apps/" + this->appID.toStdString() + "/files/lib/"
            + arch->getTriplet().toStdString()
            << std::endl;
    }
    this->cfg.mounts->push_back(ocppi::runtime::config::types::Mount{
      .destination = "/etc/ld.so.conf.d/zz_deepin-linglong-app.conf",
      .options = { { "ro", "rbind" } },
      .source = bundle.absoluteFilePath("zz_deepin-linglong-app.ld.so.conf").toStdString(),
      .type = "bind",
    });

    {
        std::ofstream ofs(bundle.absoluteFilePath("ld.so.cache").toStdString());
        Q_ASSERT(ofs.is_open());
        if (!ofs.is_open()) {
            return LINGLONG_ERR("create ld config in bundle directory");
        }
    }
    {
        std::ofstream ofs(bundle.absoluteFilePath("ld.so.cache~").toStdString());
        Q_ASSERT(ofs.is_open());
        if (!ofs.is_open()) {
            return LINGLONG_ERR("create ld config in bundle directory");
        }
    }
    this->cfg.mounts->push_back(ocppi::runtime::config::types::Mount{
      .destination = "/etc/ld.so.cache",
      .options = { { "rbind" } },
      .source = bundle.absoluteFilePath("ld.so.cache").toStdString(),
      .type = "bind",
    });
    this->cfg.mounts->push_back(ocppi::runtime::config::types::Mount{
      .destination = "/etc/ld.so.cache~",
      .options = { { "rbind" } },
      .source = bundle.absoluteFilePath("ld.so.cache~").toStdString(),
      .type = "bind",
    });

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
    ocppi::runtime::RunOption opt;
    // 禁用crun自己创建cgroup，便于AM识别和管理玲珑应用
    opt.GlobalOption::extra.push_back({ "--cgroup-manager=disabled" });
    auto result = this->cli.run(ocppi::runtime::ContainerID(this->id.toStdString()),
                                std::filesystem::path(bundle.absolutePath().toStdString()),
                                opt);

    if (!result) {
        return LINGLONG_ERR("cli run", result);
    }

    return LINGLONG_OK;
}

} // namespace linglong::runtime
