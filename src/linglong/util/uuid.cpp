/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "uuid.h"

#include <QUuid>

namespace linglong {
namespace util {
QString genUuid()
{
    return QUuid::createUuid().toString(QUuid::Id128);
}
} // namespace util
} // namespace linglong
