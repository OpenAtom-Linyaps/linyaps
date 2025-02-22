// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QString>

namespace linglong::utils {

class OverlayFS
{
public:
    OverlayFS() = delete;
    OverlayFS(const OverlayFS &) = delete;
    OverlayFS& operator=(const OverlayFS &) = delete;

    OverlayFS(QString lowerdir, QString upperdir, QString workdir, QString merged);
    ~OverlayFS();

    bool mount();
    void unmount(bool clean = false);

private:
    QString lowerdir_;
    QString upperdir_;
    QString workdir_;
    QString merged_;
};

}
