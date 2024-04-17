/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "message_reader.h"

#include "logger.h"
#include "unistd.h"

#include <fcntl.h>

namespace linglong {
namespace util {

MessageReader::MessageReader(int fd, unsigned int step)
    : fd(fd)
    , step(step)
{
    fcntl(fd, F_SETFD, FD_CLOEXEC);
}

MessageReader::~MessageReader()
{
    close(fd);
}

nlohmann::json MessageReader::read()
{
    std::unique_ptr<char[]> buf(new char[step + 1]);
    static std::string source = "";
    int ret;
    while ((ret = ::read(fd, buf.get(), step))) {
        if (ret == -1) {
            if (errno != EAGAIN) {
                logWan() << "read fail:" << errnoString();
                break;
            }
        } else {
            char *it = buf.get();
            for (; it < buf.get() + ret && *it != '\0'; it++) {
                source.push_back(*it);
            }
            if (it != buf.get() + ret && *it == '\0') {
                auto json = nlohmann::json::parse(source);
                source = std::string(it + 1, buf.get() + ret);
                return json;
            }
        }
    }

    if (!source.empty()) {
        auto json = nlohmann::json::parse(source);
        source = "";
        return json;
    }

    return nlohmann::json();
}

void MessageReader::writeChildExit(int pid, std::string cmd, int wstatus, std::string info)
{
    std::string msgTemplate =
            R"({"type":"childExit","pid":%d,"arg0":"%s","wstatus":%d,"information":"%s"})";
    auto source = util::format(msgTemplate, pid, cmd.c_str(), wstatus, info.c_str());
    write(source);
}

void MessageReader::write(std::string msg)
{
    auto pos = msg.c_str();
    do {
        int ret = ::write(fd, pos, msg.c_str() + msg.length() - pos);
        if (ret > 0)
            pos += ret;
        else {
            logWan() << "write message failed!" << errnoString();
            return;
        }
    } while (pos < msg.c_str() + msg.length());
}

} // namespace util
} // namespace linglong
