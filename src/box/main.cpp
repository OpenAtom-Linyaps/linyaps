/*
 * Copyright (c) 2020-2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <iostream>
#include <unistd.h>
#include <util/logger.h>

#include "module/oci/oci-runtime.h"
#include "container/container.h"

int main(int argc, char **argv)
{
    int readFD = atoi(argv[1]);
    std::string content;

    if (readFD <= 0) {
        std::ifstream f(argv[1]);
        std::string str((std::istreambuf_iterator<char>(f)),
                        std::istreambuf_iterator<char>());
        content = str;
    } else {
        const size_t bufSize = 1024;
        char buf[bufSize] = {};
        int ret = 0;

        do {
            ret = read(readFD, buf, bufSize);
            if (ret > 0) {
                content.append(buf, ret);
            }
        } while (ret > 0);
    }
    try {
        linglong::Container c(linglong::fromString(content));
        return c.start();
    } catch (...) {
        logErr() << "parse json failed";
        return -1;
    }
}
