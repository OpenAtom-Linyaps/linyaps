/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_BUILDER_SOURCE_FETCHER_H_
#define LINGLONG_SRC_BUILDER_SOURCE_FETCHER_H_

#include "linglong/util/error.h"
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
    explicit SourceFetcher(QSharedPointer<Source> s, Project *project);
    ~SourceFetcher() override;

    QString sourceRoot() const;

    void setSourceRoot(const QString &path);

    linglong::util::Error fetch();

    linglong::util::Error patch();

public:
    QString fixSuffix(const QFileInfo &fi);
    linglong::util::Error extractFile(const QString &path, const QString &dir);

private:
    QString srcRoot;
    QScopedPointer<SourceFetcherPrivate> dd_ptr;
    Q_DECLARE_PRIVATE_D(qGetPtrHelper(dd_ptr), SourceFetcher)
    const char *CompressedFileTarXz;
    const char *CompressedFileTarGz;
    const char *CompressedFileTarBz2;
    const char *CompressedFileTgz;
    const char *CompressedFileTar;
    const char *CompressedFileZip;
};

} // namespace builder
} // namespace linglong

#endif // LINGLONG_SRC_BUILDER_SOURCE_FETCHER_H_
