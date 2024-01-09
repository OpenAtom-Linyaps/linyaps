/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_BUILDER_SOURCE_FETCHER_H_
#define LINGLONG_SRC_BUILDER_SOURCE_FETCHER_H_

#include "linglong/util/error.h"
#include "linglong/cli/printer.h"
#include "project.h"

#include <QFileInfo>
#include <QObject>
#include <QUrl>

namespace linglong {
namespace builder {

class Source;
class Project;
class SourceFetcherPrivate;

class SourceFetcher : public QObject
{
    Q_OBJECT
public:
    explicit SourceFetcher(QSharedPointer<Source> s, cli::Printer &p, Project *project);
    ~SourceFetcher() override;

    QString sourceRoot() const;

    void setSourceRoot(const QString &path);

    linglong::util::Error fetch();

    linglong::util::Error patch();

private:
    QString srcRoot;
    cli::Printer &printer;
    QScopedPointer<SourceFetcherPrivate> dd_ptr;
    Q_DECLARE_PRIVATE_D(qGetPtrHelper(dd_ptr), SourceFetcher)
    static constexpr auto CompressedFileTarXz = "tar.xz";
    static constexpr auto CompressedFileTarGz = "tar.gz";
    static constexpr auto CompressedFileTarBz2 = "tar.bz2";
    static constexpr auto CompressedFileTgz = "tgz";
    static constexpr auto CompressedFileTar = "tar";
    static constexpr auto CompressedFileZip = "zip";
};

} // namespace builder
} // namespace linglong

#endif // LINGLONG_SRC_BUILDER_SOURCE_FETCHER_H_
