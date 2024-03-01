#include <filesystem>
#include <fstream>
#include <iostream>

#include "ytj/ytj.hpp"

namespace
{
constexpr std::string_view USAGE =
        R"(
Usage:

    ytj YAMLFILE)";

class BadUsage : public std::runtime_error {
        using std::runtime_error::runtime_error;
};
}

int main(int argc, char **argv)
try {
        std::cerr << "ytj version: " << ytj::version << std::endl;

        if (argc < 2) {
                throw BadUsage("at least 1 argument is required.");
        }

        auto file = std::filesystem::path(argv[1]);
        if (!std::filesystem::exists(file)) {
                throw std::runtime_error("file not found.");
        }
        std::ifstream input;
        input.exceptions(std::ifstream::badbit | std::ifstream::failbit);
        input.open(file);

        auto yaml = YAML::Load(input);

        auto json = ytj::to_json(yaml);

        std::cout << json.dump(4) << std::endl;

        return 0;

} catch (const BadUsage &bad_usage) {
        std::cerr << bad_usage.what() << std::endl;
        std::cerr << USAGE << std::endl;
        return -1;
} catch (const std::ifstream::failure &failure) {
        std::cerr << failure.what() << " [" << failure.code() << "]"
                  << std::endl;
        return -1;
} catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
        return -1;
}
