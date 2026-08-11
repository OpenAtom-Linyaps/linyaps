// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linglong/api/types/v1/ApplicationPermissionsRequest.hpp"
#include "linglong/api/types/v1/Generators.hpp"

#include <QWidget>

class QCloseEvent;

class PermissionDialog : public QWidget
{
    Q_OBJECT
public:
    explicit PermissionDialog(
      const linglong::api::types::v1::ApplicationPermissionsRequest &request);
    ~PermissionDialog() override = default;

protected:
    void closeEvent(QCloseEvent *event) override;
};
