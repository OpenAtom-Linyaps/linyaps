#pragma once

#include <string>
#include <vector>

#include "ocppi/runtime/GlobalOption.hpp"

namespace ocppi::runtime
{

struct StateOption : public GlobalOption {
        std::vector<std::string> extra;
};

}
