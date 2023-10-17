/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_BUILDER_BUILDER_SOURCE_FETCHER_P_H_
#define LINGLONG_SRC_BUILDER_BUILDER_SOURCE_FETCHER_P_H_

#include "module/util/file.h"
#include "module/util/http/http_client.h"
#include "module/util/runner.h"
#include "source_fetcher.h"

#include <QEventLoop>

namespace linglong {
namespace builder {

class SourceFetcherPrivate
{
public:
    explicit SourceFetcherPrivate(Source *src, SourceFetcher *parent);
    ~SourceFetcherPrivate() = default;

    QString filename();

    // TODO: use share cache for all project
    QString sourceTargetPath() const;

    std::tuple<QString, linglong::util::Error> fetchArchiveFile();

    QString fixSuffix(const QFileInfo &fi);

    linglong::util::Error extractFile(const QString &path, const QString &dir);

    util::Error fetchGitRepo();

    util::Error handleLocalSource();

    util::Error handleLocalPatch();

    Project *project;
    Source *source;
    QScopedPointer<QFile> file;

    SourceFetcher *const q_ptr;
    Q_DECLARE_PUBLIC(SourceFetcher)
};
} // namespace builder
} // namespace linglong

#endif // LINGLONG_SRC_BUILDER_BUILDER_SOURCE_FETCHER_P_H_