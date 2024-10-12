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

#include <string>

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
    // 记录linglong.yaml的位置，因为可以通过命令行参数传递，位置不再固定
    // 主要用于在构建完成后将linglong.yaml复制到应用中
    std::string projectYamlFile;
    // 兼容选项，在制作runtime时构建全量develop, 以兼容旧版本linglong-builder使用
    // TODO 后续版本删除该选项
    bool fullDevelop = false;
    explicit Builder(const api::types::v1::BuilderProject &project,
                     const QDir &workingDir,
                     repo::OSTreeRepo &repo,
                     runtime::ContainerBuilder &containerBuilder,
                     const api::types::v1::BuilderConfig &cfg);
    auto getConfig() const noexcept -> api::types::v1::BuilderConfig;
    void setConfig(const api::types::v1::BuilderConfig &cfg) noexcept;

    auto create(const QString &projectName) -> utils::error::Result<void>;

    auto build(const QStringList &args = { "/project/linglong/entry.sh" }) noexcept
      -> utils::error::Result<void>;

    auto exportUAB(const QString &destination, const UABOption &option)
      -> utils::error::Result<void>;
    auto exportLayer(const QString &destination) -> utils::error::Result<void>;

    static auto extractLayer(const QString &layerPath, const QString &destination)
      -> utils::error::Result<void>;

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
