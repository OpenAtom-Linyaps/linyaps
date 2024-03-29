/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_BUILDER_BUILDER_LINGLONG_BUILDER_H_
#define LINGLONG_SRC_BUILDER_BUILDER_LINGLONG_BUILDER_H_

#include "builder.h"
#include "linglong/cli/printer.h"
#include "linglong/repo/ostree_repo.h"
#include "linglong/service/app_manager.h"
#include "linglong/util/error.h"
#include "linglong/utils/error/error.h"
#include "ocppi/cli/CLI.hpp"
#include "ocppi/runtime/config/types/Config.hpp"
#include "ocppi/runtime/config/types/Mount.hpp"
#include "project.h"

#include <nlohmann/json.hpp>

namespace linglong {
namespace builder {

class LinglongBuilder : public QObject, public Builder
{
    Q_OBJECT
public:
    explicit LinglongBuilder(repo::OSTreeRepo &ostree,
                             cli::Printer &p,
                             ocppi::cli::CLI &cli,
                             service::AppManager &appManager);
    util::Error config(const QString &userName, const QString &password) override;

    util::Error create(const QString &projectName) override;

    utils::error::Result<void> build() override;

    util::Error exportLayer(const QString &destination) override;

    util::Error extractLayer(const QString &layerPath, const QString &destination) override;

    util::Error exportBundle(const QString &outputFilepath, bool useLocalDir) override;

    util::Error push(const QString &repoUrl,
                     const QString &repoName,
                     const QString &channel,
                     bool pushWithDevel) override;

    util::Error import() override;

    util::Error importLayer(const QString &path) override;

    utils::error::Result<void> run() override;

    util::Error track() override;

    utils::error::Result<void> appimageConvert(const QStringList &templateArgs);

private:
    repo::OSTreeRepo &repo;
    cli::Printer &printer;
    ocppi::cli::CLI &ociCLI;
    service::AppManager &appManager;

    static auto toJSON(const ocppi::runtime::config::types::Mount &) -> nlohmann::json;
    static auto toJSON(const ocppi::runtime::config::types::Config &) -> nlohmann::json;

    linglong::utils::error::Result<QSharedPointer<Project>> buildStageProjectInit();
    linglong::utils::error::Result<ocppi::runtime::config::types::Config> buildStageConfigInit();
    linglong::utils::error::Result<void> buildStageClean(const Project &project);
    linglong::utils::error::Result<QString> buildStageDepend(Project *project);
    linglong::utils::error::Result<void> buildStageRuntime(ocppi::runtime::config::types::Config &r,
                                                           const Project &project,
                                                           const QString &overlayLowerDir,
                                                           const QString &overlayUpperDir,
                                                           const QString &overlayWorkDir,
                                                           const QString &overlayMergeDir);
    linglong::utils::error::Result<void> buildStageSource(ocppi::runtime::config::types::Config &r,
                                                          Project *project);
    linglong::utils::error::Result<void>
    buildStageEnvrionment(ocppi::runtime::config::types::Config &r,
                          const Project &project,
                          const BuilderConfig &config);
    linglong::utils::error::Result<void>
    buildStageMountHelper(ocppi::runtime::config::types::Config &r);
    linglong::utils::error::Result<void>
    buildStageIDMapping(ocppi::runtime::config::types::Config &r);
    linglong::utils::error::Result<void> buildStageRootfs(ocppi::runtime::config::types::Config &r,
                                                          const QDir &workdir,
                                                          const QString &hostBasePath);
    linglong::utils::error::Result<void> buildStageRunContainer(
      QDir workdir, ocppi::cli::CLI &cli, ocppi::runtime::config::types::Config &r);
    linglong::utils::error::Result<void> buildStageCommitBuildOutput(Project *project,
                                                                     const QString &upperdir,
                                                                     const QString &workdir);
};

// TODO: remove later
class message : public JsonSerialize
{
    Q_OBJECT;
    Q_JSON_CONSTRUCTOR(message)
public:
    Q_JSON_PROPERTY(QString, type);
    Q_JSON_PROPERTY(int, pid);
    Q_JSON_PROPERTY(QString, arg0);
    Q_JSON_PROPERTY(int, wstatus);
    Q_JSON_PROPERTY(QString, information);
};

QSERIALIZER_DECLARE(message)

} // namespace builder
} // namespace linglong

#endif // LINGLONG_SRC_BUILDER_BUILDER_LINGLONG_BUILDER_H_
