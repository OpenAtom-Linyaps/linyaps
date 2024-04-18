/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "container/container.h"
#include "util/logger.h"
#include "util/message_reader.h"
#include "util/oci_runtime.h"

#include <iostream>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    try {
        auto config = std::ifstream(argv[1]);

        auto json = nlohmann::json::parse(config);
        auto runtime = json.get<linglong::Runtime>();

        linglong::Container container(runtime);
        return container.Start();
    } catch (const std::exception &e) {
        logErr() << "failed: " << e.what();
        return -1;
    }
}
