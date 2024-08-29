/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "linglong/api/types/v1/BuilderConfig.hpp"
#include "linglong/api/types/v1/BuilderProject.hpp"
#include "linglong/repo/ostree_repo.h"
#include "linglong/runtime/container_builder.h"
#include "linglong/utils/error/error.h"

namespace linglong::builder {

struct UABOption
{
    QString iconPath;
    bool exportDevelop{ false };
    bool exportI18n{ false };
};

class Builder
{
public:
    explicit Builder(const api::types::v1::BuilderProject &project,
                     const QDir &workingDir,
                     repo::OSTreeRepo &repo,
                     runtime::ContainerBuilder &containerBuilder,
                     const api::types::v1::BuilderConfig &cfg);
    auto getConfig() const noexcept -> api::types::v1::BuilderConfig;
    void setConfig(const api::types::v1::BuilderConfig &cfg) noexcept;

    auto create(const QString &projectName) -> utils::error::Result<void>;

    auto build(const QStringList &args = {
                 "/project/linglong/entry.sh" }) noexcept -> utils::error::Result<void>;

    auto exportUAB(const QString &destination,
                   const UABOption &option) -> utils::error::Result<void>;
    auto exportLayer(const QString &destination) -> utils::error::Result<void>;

    static auto extractLayer(const QString &layerPath,
                             const QString &destination) -> utils::error::Result<void>;

    auto push(const std::string &module,
              const std::string &repoUrl = "",
              const std::string &repoName = "") -> utils::error::Result<void>;

    auto import() -> utils::error::Result<void>;

    auto importLayer(const QString &path) -> utils::error::Result<void>;

    auto run(const QStringList &args = { QString("bash") }) -> utils::error::Result<void>;

private:
    repo::OSTreeRepo &repo;
    QDir workingDir;
    api::types::v1::BuilderProject project;
    runtime::ContainerBuilder &containerBuilder;
    api::types::v1::BuilderConfig cfg;
};

} // namespace linglong::builder
