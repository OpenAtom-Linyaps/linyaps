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
    enum Stream : unsigned int {
        Stdin = 1U << STDIN_FILENO,
        Stdout = 1U << STDOUT_FILENO,
        Stderr = 1U << STDERR_FILENO,
        All = Stdin | Stdout | Stderr,
    };

    std::optional<Pipe> stdinPipe;
    std::optional<Pipe> stdoutPipe;
    std::optional<Pipe> stderrPipe;

    static std::optional<PipeSet> create(int flags = O_CLOEXEC, unsigned int streams = All) noexcept
    {
        std::optional<Pipe> stdinPipe;
        if (streams & Stdin) {
            stdinPipe = Pipe::create(flags);
            if (!stdinPipe) {
                return std::nullopt;
            }
        }

        std::optional<Pipe> stdoutPipe;
        if (streams & Stdout) {
            stdoutPipe = Pipe::create(flags);
            if (!stdoutPipe) {
                return std::nullopt;
            }
        }

        std::optional<Pipe> stderrPipe;
        if (streams & Stderr) {
            stderrPipe = Pipe::create(flags);
            if (!stderrPipe) {
                return std::nullopt;
            }
        }

        return PipeSet{ std::move(stdinPipe), std::move(stdoutPipe), std::move(stderrPipe) };
    }

    [[nodiscard]] bool setupChildStdio() noexcept
    {
        // Close unused ends before they can alias a redirected standard descriptor.
        closeParentEnds();
        const auto redirect = [](int source, int target) noexcept {
            if (source == target) {
                // dup2(fd, fd) would leave FD_CLOEXEC set. Keep this descriptor open.
                return ::fcntl(target, F_SETFD, 0) != -1;
            }
            const auto result = ::dup2(source, target);
            ::close(source);
            return result != -1;
        };
        return (!stdinPipe || redirect(stdinPipe->releaseReadEnd(), STDIN_FILENO))
          && (!stdoutPipe || redirect(stdoutPipe->releaseWriteEnd(), STDOUT_FILENO))
          && (!stderrPipe || redirect(stderrPipe->releaseWriteEnd(), STDERR_FILENO));
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
