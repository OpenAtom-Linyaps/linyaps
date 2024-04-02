/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_BUILDER_BUILDER_LINGLONG_BUILDER_H_
#define LINGLONG_SRC_BUILDER_BUILDER_LINGLONG_BUILDER_H_

#include "linglong/api/types/v1/BuilderConfig.hpp"
#include "linglong/api/types/v1/BuilderProject.hpp"
#include "linglong/repo/ostree_repo.h"
#include "linglong/runtime/container_builder.h"
#include "linglong/utils/error/error.h"

namespace linglong::builder {

class Builder
{
public:
    explicit Builder(const api::types::v1::BuilderProject &project,
                     QDir workingDir,
                     repo::OSTreeRepo &repo,
                     runtime::ContainerBuilder &containerBuilder,
                     const api::types::v1::BuilderConfig &cfg);
    auto getConfig() const noexcept -> api::types::v1::BuilderConfig;
    void setConfig(const api::types::v1::BuilderConfig &cfg) noexcept;

    auto create(const QString &projectName) -> utils::error::Result<void>;

    auto build(const QStringList &args = { "/project/linglong/entry.sh" }) noexcept
      -> utils::error::Result<void>;

    auto exportLayer(const QString &destination) -> utils::error::Result<void>;

    auto extractLayer(const QString &layerPath, const QString &destination)
      -> utils::error::Result<void>;

    auto push(bool pushWithDevel = true, const QString &repoUrl = "", const QString &repoName = "")
      -> utils::error::Result<void>;

    auto import() -> utils::error::Result<void>;

    auto importLayer(const QString &path) -> utils::error::Result<void>;

    auto run(const QStringList &args = { QString("bash") }) -> utils::error::Result<void>;

    auto appimageConvert(const QStringList &templateArgs) -> utils::error::Result<void>;

private:
    auto splitDevelop(QDir developOutput, QDir runtimeOutput, QString prefix)
      -> utils::error::Result<void>;

    repo::OSTreeRepo &repo;
    QDir workingDir;
    api::types::v1::BuilderProject project;
    runtime::ContainerBuilder &containerBuilder;
    api::types::v1::BuilderConfig cfg;
};

} // namespace linglong::builder

#endif // LINGLONG_SRC_BUILDER_BUILDER_LINGLONG_BUILDER_H_
