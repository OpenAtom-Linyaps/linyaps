#pragma once

#include <map>
#include <string>

#include "ocppi/runtime/GlobalOption.hpp"

namespace ocppi::runtime
{

struct ExecOption : public GlobalOption {
        bool tty;
        std::optional<int> uid;
        std::optional<int> gid;
        std::optional<std::filesystem::path> cwd;
        std::map<std::string, std::string> env;
        std::vector<std::string> extra;
};

}
