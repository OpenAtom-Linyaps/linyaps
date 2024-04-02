/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/runtime/container.h"

#include "linglong/utils/finally/finally.h"
#include "ocppi/runtime/config/types/Generators.hpp"

#include <QDir>
#include <QStandardPaths>

#include <fstream>

#include <sys/stat.h>

namespace linglong::runtime {

Container::Container(const ocppi::runtime::config::types::Config &cfg,
                     const QString &conatinerID,
                     ocppi::cli::CLI &cli)
    : cfg(cfg)
    , id(conatinerID)
    , cli(cli)
{
    Q_ASSERT(!cfg.process.has_value());
}

utils::error::Result<void>
Container::run(const ocppi::runtime::config::types::Process &process) noexcept
{
    LINGLONG_TRACE(QString("run container %1").arg(this->id));

    QDir runtimeDir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    QDir bundle = runtimeDir.absoluteFilePath(QString("linglong/%1").arg(this->id));
    Q_ASSERT(bundle.exists());
    if (!bundle.mkpath(".")) {
        return LINGLONG_ERR("make bundle directory");
    }

    auto _ = utils::finally::finally([&]() {
        if (bundle.removeRecursively()) {
            return;
        }

        qCritical() << "failed to remove" << runtimeDir.absolutePath();
    });

    this->cfg.process = process;
    if (process.user) {
        qWarning() << "`user` field is ignored.";
        Q_ASSERT(false);
    }
    this->cfg.process->user = ocppi::runtime::config::types::User{
        .gid = getgid(),
        .uid = getuid(),
    };

    nlohmann::json json = this->cfg;

    {
        std::ofstream ofs(bundle.absoluteFilePath("config.json").toStdString());
        Q_ASSERT(ofs.is_open());
        if (!ofs.is_open()) {
            return LINGLONG_ERR("create config.json in bundle directory");
        }

        ofs << json.dump();
    }

    auto result = this->cli.run(ocppi::runtime::ContainerID(this->id.toStdString()),
                                std::filesystem::path(bundle.absolutePath().toStdString()));

    if (!result) {
        return LINGLONG_ERR(result);
    }

    return LINGLONG_OK;
}

} // namespace linglong::runtime
