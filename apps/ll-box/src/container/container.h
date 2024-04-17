/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_BOX_SRC_CONTAINER_CONTAINER_H_
#define LINGLONG_BOX_SRC_CONTAINER_CONTAINER_H_

#include "util/message_reader.h"
#include "util/oci_runtime.h"

#include <memory>

namespace linglong {

struct Option;
struct ContainerPrivate;

class Container
{
public:
    explicit Container(const Runtime &r, std::unique_ptr<util::MessageReader> reader);

    ~Container();

    int Start(const Option &opt);

private:
    std::unique_ptr<ContainerPrivate> dd_ptr;
};

} // namespace linglong

#endif /* LINGLONG_BOX_SRC_CONTAINER_CONTAINER_H_ */
