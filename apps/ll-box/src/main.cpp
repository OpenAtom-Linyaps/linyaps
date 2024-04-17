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
    // TODO(iceyer): move loader to ll-loader?
    bool is_load_bundle = (argc == 4);

    linglong::Option option;
    // TODO(iceyer): default in rootless
    if (geteuid() != 0) {
        option.rootless = true;
    }

    try {
        linglong::Runtime runtime;
        nlohmann::json json;
        std::unique_ptr<linglong::util::MessageReader> reader = nullptr;

        if (is_load_bundle) {
            runtime = loadBundle(argc, argv);
        } else {
            int socket = atoi(argv[1]);
            if (socket <= 0) {
                socket = open(argv[1], O_RDONLY | O_CLOEXEC);
            }

            reader.reset(new linglong::util::MessageReader(socket));

            json = reader->read();
        }

        if (linglong::util::fs::exists("/tmp/ll-debug")) {
            std::ofstream origin(linglong::util::format("/tmp/ll-debug/%d.json", getpid()));
            origin << json.dump(4);
            origin.close();
        }

        runtime = json.get<linglong::Runtime>();

        linglong::Container container(runtime, std::move(reader));
        return container.Start(option);
    } catch (const std::exception &e) {
        logErr() << "failed: " << e.what();
        return -1;
    }
}
