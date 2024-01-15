/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_BUILDER_BUILDER_DEPEND_FETCHER_H_
#define LINGLONG_SRC_BUILDER_BUILDER_DEPEND_FETCHER_H_

#include "linglong/repo/ostree_repo.h"
#include "linglong/util/error.h"
#include "linglong/cli/printer.h"

#include <QObject>

namespace linglong {
namespace builder {

class Project;
class BuildDepend;
class DependFetcherPrivate;

class DependFetcher : public QObject
{
    Q_OBJECT
public:
    explicit DependFetcher(QSharedPointer<const BuildDepend> bd,
                           repo::OSTreeRepo &repo,
                           cli::Printer &p,
                           Project *parent);
    ~DependFetcher() override;

    linglong::util::Error fetch(const QString &subPath, const QString &targetPath);

private:
    void printProgress(const uint &progress, const QString &speed);

    // FIXME: We should not use ostree repo in ll-builder, we should use the repo interface
    repo::OSTreeRepo &ostree;
    cli::Printer &printer;
    package::Ref ref;
    QScopedPointer<DependFetcherPrivate> dd_ptr;
    Q_DECLARE_PRIVATE_D(qGetPtrHelper(dd_ptr), DependFetcher)
};

} // namespace builder
} // namespace linglong

#endif // LINGLONG_SRC_BUILDER_BUILDER_DEPEND_FETCHER_H_
