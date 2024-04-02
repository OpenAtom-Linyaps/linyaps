#include "nlohmann/json.hpp"

#if NLOHMANN_JSON_VERSION_MAJOR != 3
#error require nlohmann_json compatible with 3.5.0
#endif

#if NLOHMANN_JSON_VERSION_MINOR < 5
#error require nlohmann_json compatible with 3.5.0
#endif

int main()
{
        return 0;
}
