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

#include <gtest/gtest.h>

#include "module/oci/oci-runtime.h"

TEST(OCI, Runtime)
{
    auto r = linglong::fromFile("../../test/data/demo/config-mini.json");

    EXPECT_EQ(r.version, "1.0.1");
    EXPECT_EQ(r.process.args[0], "/bin/bash");
    EXPECT_EQ(r.process.env[1], "TERM=xterm");
    EXPECT_EQ(r.mounts.at(1).options.at(1), "strictatime");
    EXPECT_EQ(r.linux.namespaces.size(), 4);
}
