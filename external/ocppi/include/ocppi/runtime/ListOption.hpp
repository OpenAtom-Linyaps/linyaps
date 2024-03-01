#pragma once

#include "ocppi/runtime/GlobalOption.hpp"

namespace ocppi::runtime
{

struct ListOption : public GlobalOption {
        enum class OutputFormat { Json, Text };

        OutputFormat format;
        std::vector<std::string> extra;
};

}
