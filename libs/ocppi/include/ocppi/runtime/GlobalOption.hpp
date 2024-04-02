#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace ocppi::runtime
{
struct GlobalOption {
        std::optional<std::filesystem::path> root;
        std::vector<std::string> extra;
};
}
