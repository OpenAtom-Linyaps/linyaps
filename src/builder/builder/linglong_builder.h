/*
 * Copyright (c) 2022. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_BUILDER_BUILDER_LINGLONG_BUILDER_H_
#define LINGLONG_SRC_BUILDER_BUILDER_LINGLONG_BUILDER_H_

#include "builder.h"
#include "module/package/bundle.h"

namespace linglong {
namespace builder {

class LinglongBuilder
    : public QObject
    , public Builder
{
    Q_OBJECT
public:
    util::Error create(const QString &projectName) override;

    util::Error build() override;

    util::Error exportBundle(const QString &outputFilepath) override;

    util::Error push(const QString &bundleFilePath, bool force) override;

    util::Error run() override;

    util::Error initRepo();
};

} // namespace builder
} // namespace linglong

#endif // LINGLONG_SRC_BUILDER_BUILDER_LINGLONG_BUILDER_H_
