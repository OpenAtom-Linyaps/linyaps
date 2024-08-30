/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "nlohmann/json.hpp"

namespace linglong {
namespace util {

// MessageReader read jsons separated by '\0' from fd.
class MessageReader
{
public:
    MessageReader(int fd, unsigned int step = 1024);
    ~MessageReader();
    nlohmann::json read();
    void write(std::string msg);
    void writeChildExit(int pid, std::string cmd, int wstatus, std::string info);
    int fd;

private:
    int step;
};
} // namespace util
} // namespace linglong
