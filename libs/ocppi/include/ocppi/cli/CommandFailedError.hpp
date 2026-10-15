#pragma once

#include <sys/wait.h>

#include <cstdlib>
#include <stdexcept>
#include <string>

namespace ocppi::cli
{

class CommandFailedError : public std::runtime_error {
        using runtime_error::runtime_error;

        static constexpr const char *prefix = "run command failed: ";
        int waitStatus_ = -1;

    public:
        explicit CommandFailedError(int ret, const std::string &command)
                : std::runtime_error(prefix + command +
                                     " retval=" + std::to_string(ret))
                , waitStatus_(ret)
        {
        }
        explicit CommandFailedError(int ret, const char *arg)
                : CommandFailedError(ret, std::string(arg))
        {
        }

        // runProcess returns the raw wait status, not an exit code.
        [[nodiscard]]
        int exitStatus() const noexcept
        {
                if (WIFEXITED(waitStatus_)) {
                        return WEXITSTATUS(waitStatus_);
                }
                if (WIFSIGNALED(waitStatus_)) {
                        return 128 + WTERMSIG(waitStatus_);
                }
                return EXIT_FAILURE;
        }
};

}
