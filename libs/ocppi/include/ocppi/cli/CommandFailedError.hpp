#pragma once

#include <stdexcept>
#include <string>

namespace ocppi::cli
{

class CommandFailedError : public std::runtime_error {
        using runtime_error::runtime_error;

        static constexpr const char *prefix = "run command failed: ";

    public:
        explicit CommandFailedError(int ret, const std::string &command)
                : std::runtime_error(prefix + command +
                                     " retval=" + std::to_string(ret))
        {
        }
        explicit CommandFailedError(int ret, const char *arg)
                : CommandFailedError(ret, std::string(arg))
        {
        }
};

}
