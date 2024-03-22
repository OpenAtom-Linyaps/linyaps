/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_BUILDER_BUILDER_SOURCE_FETCHER_P_H_
#define LINGLONG_SRC_BUILDER_BUILDER_SOURCE_FETCHER_P_H_

#include "linglong/utils/error/error.h"
#include "source_fetcher.h"

#include <QEventLoop>

namespace linglong {
namespace builder {

class SourceFetcherPrivate
{
public:
    explicit SourceFetcherPrivate(QSharedPointer<Source> src, SourceFetcher *parent);
    ~SourceFetcherPrivate() = default;

    QString filename();

    // TODO: use share cache for all project
    QString sourceTargetPath() const;

    std::tuple<QString, linglong::util::Error> fetchArchiveFile();

    QString fixSuffix(const QFileInfo &fi);
    linglong::util::Error extractFile(const QString &path, const QString &dir);

    utils::error::Result<void> fetchGitRepo();
    utils::error::Result<void> fetchDscRepo();

    util::Error handleLocalSource();

    util::Error handleLocalPatch();

    Project *project;
    QSharedPointer<Source> source;
    QScopedPointer<QFile> file;

    SourceFetcher *const q_ptr;
    Q_DECLARE_PUBLIC(SourceFetcher)
};
} // namespace builder
} // namespace linglong

#endif // LINGLONG_SRC_BUILDER_BUILDER_SOURCE_FETCHER_P_H_
