// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "permission.h"

#include <nlohmann/json.hpp>

#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTimer>

#include <iostream>

PermissionDialog::PermissionDialog(const linglong::api::types::v1::RequiredPermissions &perms)
    : QWidget(nullptr)
{
    // content
    QStringList dirs;
    std::transform(perms.directories.cbegin(),
                   perms.directories.cend(),
                   std::back_inserter(dirs),
                   [](const std::string &s) {
                       return QString::fromStdString(s);
                   });
    auto *content = new QLabel;
    content->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    content->setWordWrap(true);
    content->setMinimumSize(210, 90);
    auto contentText =
      QString{ "Whether to allow %1 to access %2?" }.arg(QString::fromStdString(perms.appID),
                                                         dirs.join(" "));
    content->setText(contentText);

    QFont contentFont;
    contentFont.setPixelSize(14);
    content->setFont(contentFont);

    // button
    auto *allowedButton = new QPushButton{ "Allow" };
    connect(allowedButton, &QPushButton::clicked, [] {
        std::cout << "ALLOW" << std::endl;
        QApplication::exit(0);
    });

    QString deniedButtonText{ "Deny (%1s)" };
    auto *deniedButton = new QPushButton{ deniedButtonText.arg(15) };
    connect(deniedButton, &QPushButton::clicked, [] {
        std::cout << "DENY" << std::endl;
        QApplication::exit(0);
    });

    using namespace std::chrono_literals;
    auto *timer = new QTimer(this);
    timer->setProperty("timeOut", 15);
    connect(timer, &QTimer::timeout, [timer, deniedButtonText, deniedButton] {
        auto timeOut = std::chrono::seconds{ timer->property("timeOut").toInt() };
        if (timeOut == 0s) {
            std::cout << "DENY" << std::endl;
            QApplication::exit(0);
        }

        auto newTimeOut = timeOut - 1s;
        deniedButton->setText(deniedButtonText.arg(newTimeOut.count()));
        timer->setProperty("timeOut", QVariant::fromValue(newTimeOut.count()));
        timer->start(1s);
    });

    auto *buttonLayout = new QHBoxLayout;
    buttonLayout->addWidget(allowedButton);
    buttonLayout->addWidget(deniedButton);

    auto *layout = new QVBoxLayout{ this };
    layout->addWidget(content);
    layout->addLayout(buttonLayout);

    setLayout(layout);
    timer->start(1s);
}
