/*
 * Copyright (c) 2022. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_BUILDER_BUILDER_BUILDER_CONFIG_H_
#define LINGLONG_SRC_BUILDER_BUILDER_BUILDER_CONFIG_H_

#include <QString>
#include <QStandardPaths>

#include "module/util/singleton.h"

namespace linglong {
namespace builder {

/*!
 * Builder is the global config of the builder system
 */
class BuilderConfig : public linglong::util::Singleton<BuilderConfig>
{
    friend class linglong::util::Singleton<BuilderConfig>;

public:
    QString repoPath() const;

    QString ostreePath() const;

    // TODO: remove later
    QString layerPath(const QStringList &subPathList) const;

    // TODO: move to option
    void setProjectRoot(const QString &path);
    QString projectRoot() const;

    void setExec(const QString &exec);
    QString exec() const;

    // TODO: remove later
    QString targetFetchCachePath() const;

    // TODO: remove later
    QString targetSourcePath() const;

    QString templatePath() const;
private:
    QString m_projectRoot;
    QString m_exec;
};
} // namespace builder
} // namespace linglong

#endif // LINGLONG_SRC_BUILDER_BUILDER_BUILDER_CONFIG_H_
