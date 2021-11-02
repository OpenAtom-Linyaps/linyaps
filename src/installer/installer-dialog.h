/*
 * Copyright (c) 2021 Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QMainWindow>
#include <QScopedPointer>

namespace linglong {
namespace installer {

class InstallerDialogPrivate;
class InstallerDialog : public QMainWindow
{
    Q_OBJECT

public:
    explicit InstallerDialog(QWidget *parent = nullptr);
    ~InstallerDialog() override;

    void install(const QString &appID);

private:
    QScopedPointer<InstallerDialogPrivate> dd_ptr;
    Q_DECLARE_PRIVATE_D(qGetPtrHelper(dd_ptr), InstallerDialog)
};

} // namespace installer
} // namespace linglong
