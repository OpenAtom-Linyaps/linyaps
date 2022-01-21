/*
 * Copyright (c) 2022. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_BUILDER_SOURCE_FETCHER_H_
#define LINGLONG_SRC_BUILDER_SOURCE_FETCHER_H_

#include <QObject>

#include "module/util/result.h"

namespace linglong {
namespace builder {

class Source;
class Project;
class SourceFetcherPrivate;
class SourceFetcher : public QObject
{
    Q_OBJECT
public:
    explicit SourceFetcher(Source *s, Project *project);
    ~SourceFetcher() override;

    QString sourceRoot();

    linglong::util::Error fetch();

private:
    QScopedPointer<SourceFetcherPrivate> dd_ptr;
    Q_DECLARE_PRIVATE_D(qGetPtrHelper(dd_ptr), SourceFetcher)
};

} // namespace builder
} // namespace linglong

#endif // LINGLONG_SRC_BUILDER_SOURCE_FETCHER_H_
