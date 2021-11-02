/*
 * Copyright (c) 2021 Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "installer-dialog.h"

#include <QLabel>
#include <QPlainTextEdit>
#include <QBoxLayout>
#include <QProcess>
#include <QScrollBar>
#include <QPushButton>
#include <QDebug>

namespace linglong {
namespace installer {

class InstallerDialogPrivate
{
    explicit InstallerDialogPrivate(InstallerDialog *parent = nullptr)
        : q_ptr(parent)
    {
    }
    QLabel *titleLabel;
    QPlainTextEdit *logLabel;
    QPushButton *actionButton;
    QString appID;

    QProcess og;

    InstallerDialog *q_ptr;
    Q_DECLARE_PUBLIC(InstallerDialog);
};

InstallerDialog::InstallerDialog(QWidget *parent)
    : QMainWindow(parent)
    , dd_ptr(new InstallerDialogPrivate(this))
{
    Q_D(InstallerDialog);

    setWindowTitle("Linglong Installer");

    auto w = new QWidget(this);

    auto l = new QVBoxLayout(w);

    auto topLayout = new QHBoxLayout();

    d->titleLabel = new QLabel("");
    topLayout->addWidget(d->titleLabel);

    d->actionButton = new QPushButton(tr("Run"));
    topLayout->addWidget(d->actionButton);
    d->actionButton->hide();

    topLayout->setStretchFactor(d->titleLabel, 100);
    topLayout->setStretchFactor(d->actionButton, 0);

    l->addLayout(topLayout);

    d->logLabel = new QPlainTextEdit("");
    l->addWidget(d->logLabel);

    l->setStretchFactor(d->logLabel, 100);

    setCentralWidget(w);

    connect(d->actionButton, &QPushButton::released, [=]() { QProcess::startDetached("ll-cmd", {"run", d->appID}); });
}

InstallerDialog::~InstallerDialog()
{
}

void InstallerDialog::install(const QString &appID)
{
    Q_D(InstallerDialog);
    d->appID = appID;
    d->titleLabel->setText(appID + " installing...");

    d->og.setProgram("ll-cmd");

    QStringList ostreeArgs = {"install", appID};
    d->og.setArguments(ostreeArgs);

    d->og.connect(&d->og, &QProcess::readyReadStandardOutput, [=]() {
        qCritical() << "readyReadStandardOutput";
        d->logLabel->appendPlainText(QString::fromLocal8Bit(d->og.readAllStandardOutput()));
        d->logLabel->verticalScrollBar()->setValue(d->logLabel->verticalScrollBar()->maximum());
    });

    d->og.connect(&d->og, &QProcess::readyReadStandardError, [=]() {
        qCritical() << "readyReadStandardError";
        d->logLabel->appendPlainText(QString::fromLocal8Bit(d->og.readAllStandardError()));
        d->logLabel->verticalScrollBar()->setValue(d->logLabel->verticalScrollBar()->maximum());
    });

    d->og.connect(&d->og, static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished), [=]() {
        qCritical() << "finished";
        qCritical() << d->og.exitStatus() << "with exit code:" << d->og.exitCode() << d->og.error();
        if (d->og.exitCode() != 0) {
            d->titleLabel->setText(appID + " install failed");
            d->actionButton->hide();
        } else {
            d->titleLabel->setText(appID + " install success");
            d->actionButton->show();
        }
    });

    qDebug() << "start" << d->og.arguments().join(" ");
    d->og.start();
}

} // namespace installer
} // namespace linglong