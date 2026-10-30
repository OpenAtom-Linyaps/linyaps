#pragma once

#include <sys/wait.h>

#include <stdexcept>
#include <string>

namespace ocppi::cli
{

class CommandFailedError : public std::runtime_error {
        using runtime_error::runtime_error;

        static constexpr const char *prefix = "run command failed: ";

        [[nodiscard]]
        static auto exitCode(int ret) noexcept -> int
        {
                if (WIFEXITED(ret)) {
                        return WEXITSTATUS(ret);
                }
                if (WIFSIGNALED(ret)) {
                        return WTERMSIG(ret) + 128;
                }
                return ret;
        }

    public:
        explicit CommandFailedError(int ret, const std::string &command)
                : std::runtime_error(prefix + command +
                                     " retval=" + std::to_string(exitCode(ret)))
                , ret_(exitCode(ret))
        {
        }
        explicit CommandFailedError(int ret, const char *arg)
                : CommandFailedError(ret, std::string(arg))
        {
        }

        [[nodiscard]]
        auto exitStatus() const noexcept -> int
        {
                return this->ret_;
        }

    private:
        int ret_{ 0 };
};

}
