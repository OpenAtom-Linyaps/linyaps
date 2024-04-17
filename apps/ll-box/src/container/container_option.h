/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_BOX_SRC_CONTAINER_CONTAINER_OPTION_H_
#define LINGLONG_BOX_SRC_CONTAINER_CONTAINER_OPTION_H_

#include "util/json.h"

namespace linglong {

/*!
 * It's seem not a good idea to add extra config option.
 * The oci runtime json should contain option, but now it's hard to modify
 * the work flow.
 * If there is more information about how to deal user namespace, it can remove
 */
struct Option
{
    bool rootless = false;
    bool linkLfs = true;
};

} // namespace linglong

#endif /* LINGLONG_BOX_SRC_CONTAINER_CONTAINER_OPTION_H_ */
