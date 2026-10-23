// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linglong/utils/unique_fd.h"

#include <array>
#include <cerrno>
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

    [[nodiscard]] bool setupChildStdio() noexcept
    {
        // Stage every source before closing or overwriting any descriptor. Pipe
        // endpoints may occupy 0..2 when the caller started with closed stdio.
        const std::array<int, 3> sources{
            stdinPipe ? stdinPipe->readEnd() : -1,
            stdoutPipe ? stdoutPipe->writeEnd() : -1,
            stderrPipe ? stderrPipe->writeEnd() : -1,
        };
        std::array<fd::UniqueFd, 3> staged;
        for (std::size_t i = 0; i < sources.size(); ++i) {
            const bool configured = i == 0 ? stdinPipe.has_value()
              : i == 1                     ? stdoutPipe.has_value()
                                           : stderrPipe.has_value();
            if (!configured) {
                continue;
            }
            int duplicate;
            do {
                duplicate = ::fcntl(sources[i], F_DUPFD_CLOEXEC, STDERR_FILENO + 1);
            } while (duplicate < 0 && errno == EINTR);
            if (duplicate < 0) {
                return false;
            }
            staged[i].reset(duplicate);
        }

        // Close originals before dup2: closing an original numbered 0..2 after
        // redirection could otherwise close a newly configured standard FD.
        for (auto *pipe : { &stdinPipe, &stdoutPipe, &stderrPipe }) {
            if (*pipe) {
                (*pipe)->closeReadEnd();
                (*pipe)->closeWriteEnd();
            }
        }
        for (std::size_t i = 0; i < staged.size(); ++i) {
            if (!staged[i]) {
                continue;
            }
            int result;
            do {
                // Sources are >= 3, so dup2 also clears FD_CLOEXEC on 0..2.
                result = ::dup2(staged[i].get(), static_cast<int>(i));
            } while (result < 0 && errno == EINTR);
            if (result < 0) {
                return false;
            }
        }
        return true;
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
            stdinPipe->closeReadEnd();
        }
        if (stdoutPipe) {
            stdoutPipe->closeWriteEnd();
        }
        if (stderrPipe) {
            stderrPipe->closeWriteEnd();
        }
    }
};

} // namespace linglong::utils::io
