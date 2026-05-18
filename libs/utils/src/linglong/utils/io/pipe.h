// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linglong/utils/unique_fd.h"

#include <array>
#include <optional>

#include <fcntl.h>
#include <unistd.h>

namespace linglong::utils::io {

class Pipe
{
public:
    static std::optional<Pipe> create(int flags = 0) noexcept
    {
        std::array<int, 2> fds{ -1, -1 };
        if (::pipe2(fds.data(), flags) != 0) {
            return std::nullopt;
        }
        return Pipe{ fds[0], fds[1] };
    }

    [[nodiscard]] int readEnd() const noexcept { return readEnd_.get(); }

    [[nodiscard]] int writeEnd() const noexcept { return writeEnd_.get(); }

    int releaseReadEnd() noexcept { return readEnd_.release(); }

    int releaseWriteEnd() noexcept { return writeEnd_.release(); }

    void closeWriteEnd() noexcept { writeEnd_.reset(); }

    void closeReadEnd() noexcept { readEnd_.reset(); }

    [[nodiscard]] explicit operator bool() const noexcept
    {
        return static_cast<bool>(readEnd_) || static_cast<bool>(writeEnd_);
    }

private:
    Pipe(int readFd, int writeFd) noexcept
        : readEnd_(readFd)
        , writeEnd_(writeFd)
    {
    }

    fd::UniqueFd readEnd_;
    fd::UniqueFd writeEnd_;
};

struct PipeSet
{
    std::optional<Pipe> stdinPipe;
    std::optional<Pipe> stdoutPipe;
    std::optional<Pipe> stderrPipe;

    static std::optional<PipeSet> create(int flags = O_CLOEXEC) noexcept
    {
        auto stdinPipe = Pipe::create(flags);
        if (!stdinPipe) {
            return std::nullopt;
        }

        auto stdoutPipe = Pipe::create(flags);
        if (!stdoutPipe) {
            return std::nullopt;
        }

        auto stderrPipe = Pipe::create(flags);
        if (!stderrPipe) {
            return std::nullopt;
        }

        return PipeSet{ std::move(stdinPipe), std::move(stdoutPipe), std::move(stderrPipe) };
    }

    void setupChildStdio() noexcept
    {
        if (stdinPipe) {
            ::dup2(stdinPipe->readEnd(), STDIN_FILENO);
            stdinPipe->closeReadEnd();
        }
        if (stdoutPipe) {
            ::dup2(stdoutPipe->writeEnd(), STDOUT_FILENO);
            stdoutPipe->closeWriteEnd();
        }
        if (stderrPipe) {
            ::dup2(stderrPipe->writeEnd(), STDERR_FILENO);
            stderrPipe->closeWriteEnd();
        }
    }

    void closeChildEnds() noexcept
    {
        if (stdinPipe) {
            stdinPipe->closeReadEnd();
        }
        if (stdoutPipe) {
            stdoutPipe->closeWriteEnd();
        }
        if (stderrPipe) {
            stderrPipe->closeWriteEnd();
        }
    }

    void closeParentEnds() noexcept
    {
        if (stdinPipe) {
            stdinPipe->closeWriteEnd();
        }
        if (stdoutPipe) {
            stdoutPipe->closeReadEnd();
        }
        if (stderrPipe) {
            stderrPipe->closeReadEnd();
        }
    }
};

} // namespace linglong::utils::io
