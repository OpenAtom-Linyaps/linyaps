/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "mainwindow.h"

#include "ui_mainwindow.h"

#include <QtGui>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    this->setWindowTitle("@APP_ID@");
    auto parentRect = QGuiApplication::primaryScreen()->availableGeometry();
    if (this->parentWidget()) {
        parentRect = this->parentWidget()->geometry();
    }

    this->move(parentRect.center() - this->rect().center());
}

MainWindow::~MainWindow()
{
    delete ui;
}