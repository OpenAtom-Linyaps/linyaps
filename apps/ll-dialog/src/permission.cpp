// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "permission.h"

#include "linglong/api/types/v1/ChosenPermissions.hpp"

#include <nlohmann/json.hpp>

#include <QButtonGroup>
#include <QCheckBox>
#include <QDebug>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>

#include <algorithm>
#include <iostream>

PermissionDialog::PermissionDialog(const linglong::api::types::v1::RequiredPermissions &perms)
    : QWidget(nullptr)
{
    // summary
    auto *summary = new QLabel;
    summary->setAlignment(Qt::AlignCenter);
    auto summaryText =
      QString{ "%1 required to access below directory:" }.arg(QString::fromStdString(perms.appID));
    summary->setText(summaryText);

    QFont summaryFont;
    summaryFont.setPixelSize(14);
    summary->setFont(summaryFont);

    static QStringList directories{ "Documents", "Downloads", "Desktop",
                                    "Music",     "Pictures",  "Videos" };
    // directory
    QFont checkBoxFont;
    checkBoxFont.setPixelSize(13);
    checkBoxFont.setWeight(QFont::Normal);
    auto *directoryList = new QListWidget;
    auto *directoryGroup = new QButtonGroup;
    directoryGroup->setExclusive(false);
    for (const auto &dir : directories) {
        auto *listItem = new QListWidgetItem;
        auto *checkBox = new QCheckBox;
        checkBox->setText(dir);
        checkBox->setProperty("dirKey", dir);
        checkBox->setFont(checkBoxFont);
        directoryGroup->addButton(checkBox);
        listItem->setSizeHint(checkBox->sizeHint());
        directoryList->addItem(listItem);
        directoryList->setItemWidget(listItem, checkBox);
    }

    connect(directoryGroup,
            QOverload<QAbstractButton *, bool>::of(&QButtonGroup::buttonToggled),
            [this](QAbstractButton *box, bool checked) {
                auto *checkBox = qobject_cast<QCheckBox *>(box);
                if (!checked) {
                    this->allowedDirectory.erase(
                      std::remove(this->allowedDirectory.begin(),
                                  this->allowedDirectory.end(),
                                  checkBox->property("dirKey").toString().toStdString()),
                      this->allowedDirectory.end());
                } else {
                    this->allowedDirectory.emplace_back(
                      checkBox->property("dirKey").toString().toStdString());
                }
            });

    auto *contentLayout = new QVBoxLayout;
    contentLayout->addWidget(summary);
    contentLayout->addWidget(directoryList);

    // button
    auto *okButton = new QPushButton{ "OK" };
    okButton->setEnabled(false);
    connect(directoryGroup,
            QOverload<QAbstractButton *, bool>::of(&QButtonGroup::buttonToggled),
            [this, okButton](QAbstractButton *, bool) {
                okButton->setEnabled(!this->allowedDirectory.empty());
            });

    connect(okButton, &QPushButton::clicked, [this] {
        std::sort(this->allowedDirectory.begin(), this->allowedDirectory.end());
        linglong::api::types::v1::ChosenPermissions perms{ .directories = this->allowedDirectory };
        std::cout << nlohmann::json(perms).dump() << std::endl;
        this->close();
    });

    auto *cancelButton = new QPushButton{ "Cancel" };
    connect(cancelButton, &QPushButton::clicked, [this] {
        std::cout << nlohmann::json(linglong::api::types::v1::ChosenPermissions{}).dump()
                  << std::endl;
        this->close();
    });

    auto *buttonLayout = new QHBoxLayout;
    buttonLayout->addWidget(okButton);
    buttonLayout->addWidget(cancelButton);

    auto *layout = new QVBoxLayout{ this };
    layout->addLayout(contentLayout);
    layout->addLayout(buttonLayout);

    setLayout(layout);
}
