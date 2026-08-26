// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <sys/stat.h>

class ScopedUmask
{
public:
    explicit ScopedUmask(mode_t mask)
        : previousMask(::umask(mask))
    {
    }

    ~ScopedUmask() { ::umask(this->previousMask); }

    ScopedUmask(const ScopedUmask &) = delete;
    ScopedUmask &operator=(const ScopedUmask &) = delete;

private:
    mode_t previousMask;
};
