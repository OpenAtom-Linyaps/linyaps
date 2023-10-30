/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_BUILDER_BUILDER_BUILDER_CONFIG_H_
#define LINGLONG_SRC_BUILDER_BUILDER_BUILDER_CONFIG_H_

#include "linglong/util/qserializer/deprecated.h"

#include <QStandardPaths>
#include <QString>

namespace linglong {
namespace builder {

namespace PrivateBuilderConfigInit {
int init();
static int _ = init();
} // namespace PrivateBuilderConfigInit

/*!
 * Builder is the global config of the builder system
 */
class BuilderConfig : public Serialize
{
    Q_OBJECT;
    // FIXME: it seam an fake singleton, and ref to fromVariant may fix this.
    Q_JSON_CONSTRUCTOR(BuilderConfig)
public:
    static BuilderConfig *instance();

    Q_SERIALIZE_PROPERTY(QString, remoteRepoEndpoint);
    Q_SERIALIZE_PROPERTY(QString, remoteRepoName);

    QString repoPath() const;

    QString ostreePath() const;

    // TODO: remove later
    QString layerPath(const QStringList &subPathList) const;

    // TODO: move to option
    void setProjectRoot(const QString &path);
    QString getProjectRoot() const;

    void setProjectName(const QString &name);
    QString getProjectName() const;

    void setExec(const QStringList &exec);
    QStringList getExec() const;

    void setOffline(const bool &exec);
    bool getOffline() const;

    // TODO: remove later
    QString targetFetchCachePath() const;

    // TODO: remove later
    QString targetSourcePath() const;

    QString templatePath() const;

private:
    QString projectRoot;
    QString projectName;
    QStringList exec;
    bool offline;
};

QSERIALIZER_DECLARE(BuilderConfig)

} // namespace builder
} // namespace linglong

#endif // LINGLONG_SRC_BUILDER_BUILDER_BUILDER_CONFIG_H_
