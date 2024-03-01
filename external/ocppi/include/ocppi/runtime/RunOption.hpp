#pragma once

#include <string>
#include <vector>

#include "ocppi/runtime/GlobalOption.hpp"

namespace ocppi::runtime
{

struct RunOption : public GlobalOption {
        std::vector<std::string> extra;
};

}
