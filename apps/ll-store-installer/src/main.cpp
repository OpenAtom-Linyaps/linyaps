/*
 * SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <QApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QScreen>
#include <QUrl>

#include "installer-dialog.h"

void moveToCenter(QWidget *w)
{
    Q_ASSERT(w != nullptr);

    auto parentRect = QGuiApplication::primaryScreen()->availableGeometry();
    if (w->parentWidget()) {
        parentRect = w->parentWidget()->geometry();
    }

    w->move(parentRect.center() - w->rect().center());
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    QCommandLineParser parser;

    parser.addHelpOption();
    parser.addPositionalArgument("app-id", "appID", "appID");

    parser.parse(QCoreApplication::arguments());

    QStringList args = parser.positionalArguments();
    QUrl appURI(args.isEmpty() ? QString() : args.first());
    QString appID = appURI.host();

    linglong::installer::InstallerDialog id;

    id.setFixedSize(600, 370);
    moveToCenter(&id);
    id.show();

    id.install(appID);

    return app.exec();
}
