/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_BUILDER_BUILDER_DEPEND_FETCHER_H_
#define LINGLONG_SRC_BUILDER_BUILDER_DEPEND_FETCHER_H_

#include "module/util/error.h"

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
    explicit DependFetcher(QSharedPointer<const BuildDepend> bd, Project *parent);
    ~DependFetcher() override;

    linglong::util::Error fetch(const QString &subPath, const QString &targetPath);

private:
    QScopedPointer<DependFetcherPrivate> dd_ptr;
    Q_DECLARE_PRIVATE_D(qGetPtrHelper(dd_ptr), DependFetcher)
};

} // namespace builder
} // namespace linglong

#endif // LINGLONG_SRC_BUILDER_BUILDER_DEPEND_FETCHER_H_
