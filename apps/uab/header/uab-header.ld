/* SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd. */
/* SPDX-License-Identifier: LGPL-3.0-or-later */

SECTIONS
{
    .note.uab.sig : ALIGN(4)
    {
        KEEP(*(.text.uab.sig))
    }
}
INSERT AFTER .text;
