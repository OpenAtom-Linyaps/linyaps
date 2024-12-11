// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "cache_dialog.h"

#include "linglong/utils/gettext.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPalette>
#include <QPixmap>
#include <QScreen>
#include <QTimer>
#include <QVBoxLayout>

CacheDialog::CacheDialog(QString id)
    : QWidget(nullptr)
    , id(std::move(id))
{
    // set widget icon
    QIcon icon(":/logo.svg");
    this->setWindowIcon(icon);
    this->setWindowFlags(Qt::Window | Qt::FramelessWindowHint);

    // set widget position and geometry
    QSize size(660, 430);
    auto *desktop = QGuiApplication::primaryScreen();
    auto screenSize = desktop->size();
    int x = (screenSize.width() - size.width()) / 2;
    int y = (screenSize.height() - size.height()) / 2;
    this->setFixedSize(size);
    this->move(x, y);

    // set widget background color
    QPalette palette = this->palette();
    palette.setColor(QPalette::Window, QColor(248, 248, 248));
    this->setPalette(palette);

    // add main layout
    QVBoxLayout *mainLayout = new QVBoxLayout();
    QHBoxLayout *hLayout = new QHBoxLayout();
    mainLayout->setContentsMargins(0, 0, 0, 0);

    // add picture label
    QPixmap p(":/background.jpeg");
    QLabel *backgroundLabel = new QLabel();
    backgroundLabel->setGeometry(0, 0, this->width(), this->height());
    backgroundLabel->setPixmap(p.scaled(660, 350));

    QLabel *textLabel = new QLabel(_("Linglong Package Manager"));
    QLabel *tipsLabel = new QLabel(this->id + " " + _("is starting"));
    QLabel *dotLabel = new QLabel("...");
    dotLabel->setFixedSize(30, 25);

    QFont textFont = textLabel->font();
    textFont.setPointSize(12);
    textFont.setBold(true);
    textLabel->setFont(textFont);

    QFont tipsFont = tipsLabel->font();
    tipsFont.setPointSize(11);
    tipsLabel->setFont(tipsFont);

    textLabel->setAlignment(Qt::AlignCenter | Qt::AlignBottom);
    tipsLabel->setAlignment(Qt::AlignCenter | Qt::AlignBottom);
    dotLabel->setAlignment(Qt::AlignLeft | Qt::AlignBottom);

    hLayout->setSpacing(2);
    hLayout->addSpacing(32);
    hLayout->addWidget(tipsLabel);
    hLayout->addWidget(dotLabel);
    hLayout->setAlignment(Qt::AlignCenter);

    mainLayout->addWidget(backgroundLabel);
    mainLayout->addSpacing(15);
    mainLayout->addWidget(textLabel);
    mainLayout->addLayout(hLayout);
    mainLayout->addSpacing(30);
    this->setLayout(mainLayout);

    QTimer *timer = new QTimer();
    timer->setInterval(800);
    connect(timer, &QTimer::timeout, [this, dotLabel]() {
        QString dots;
        for (int i = 0; i < dotCount % 3 + 1; ++i) {
            dots += ".";
        }
        this->dotCount++;
        dotLabel->setText(dots);
    });

    timer->start();
}
