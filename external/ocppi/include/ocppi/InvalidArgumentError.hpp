#pragma once

#include <stdexcept>
#include <string>

namespace ocppi::common
{

class InvalidArgumentError : public std::invalid_argument {
        using invalid_argument::invalid_argument;

        static constexpr const char *prefix = "invalid argument: ";

    public:
        explicit InvalidArgumentError(const std::string &arg)
                : std::invalid_argument(prefix + arg)
        {
        }
        explicit InvalidArgumentError(const char *arg)
                : std::invalid_argument(prefix + std::string(arg))
        {
        }
};

}
