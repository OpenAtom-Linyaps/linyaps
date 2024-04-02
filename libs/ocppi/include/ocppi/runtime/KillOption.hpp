#pragma once

#include <string>
#include <vector>

#include "ocppi/runtime/GlobalOption.hpp"

namespace ocppi::runtime
{

struct KillOption : public GlobalOption {
        std::vector<std::string> extra;
};

}
