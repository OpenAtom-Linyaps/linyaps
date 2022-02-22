/*
 * Copyright (c) 2022 Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_SERVICE_IMPL_PACKAGE_MANAGER_OPTION_H_
#define LINGLONG_SRC_SERVICE_IMPL_PACKAGE_MANAGER_OPTION_H_

#include "module/util/json.h"

namespace linglong {
namespace service {

// pass args with PackageManagerOption between dbus
class PackageManagerOption : public JsonSerialize
{
    Q_OBJECT;
    Q_JSON_CONSTRUCTOR(PackageManagerOption);

public:
    Q_JSON_PROPERTY(QString, ref);
};

} // namespace service
} // namespace linglong

Q_JSON_DECLARE_PTR_METATYPE_NM(linglong::service, PackageManagerOption)

namespace linglong {
namespace service {

// wrapOption will pass a custom qt class to dbus in QList<QPinter<QCustomObject>>
// there maybe other perfect method to do that, but now this is work :<
inline PackageManagerOptionList wrapOption(const QPointer<PackageManagerOption> &opt)
{
    PackageManagerOptionList list;
    list.push_back(opt);
    return list;
}

inline QPointer<PackageManagerOption> unwrapOption(const PackageManagerOptionList &list)
{
    return list.value(0);
}

} // namespace service
} // namespace linglong

#endif // LINGLONG_SRC_SERVICE_IMPL_PACKAGE_MANAGER_OPTION_H_
