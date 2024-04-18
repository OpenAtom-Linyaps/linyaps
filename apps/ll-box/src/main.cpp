/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "container/container.h"
#include "container/container_option.h"
#include "util/logger.h"
#include "util/message_reader.h"
#include "util/oci_runtime.h"

#include <iostream>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

extern linglong::Runtime loadBundle(int argc, char **argv);

int main(int argc, char **argv)
{
    linglong::Option option;

    option.rootless = true;

    try {
        linglong::Runtime runtime;
        nlohmann::json json;

        auto config = std::ifstream(argv[1]);

        json = nlohmann::json::parse(config);
        runtime = json.get<linglong::Runtime>();

        linglong::Container container(runtime);
        return container.Start(option);
    } catch (const std::exception &e) {
        logErr() << "failed: " << e.what();
        return -1;
    }
}
