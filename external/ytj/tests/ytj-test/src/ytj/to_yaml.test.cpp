#include "ytj/find_catch2.hpp"
#include "ytj/ytj.hpp"

using nlohmann::json;
using ytj::to_json;
using ytj::to_yaml;

TEST_CASE("to_yaml")
{
        SECTION("Scalar")
        {
                const std::vector<json> cases{
                        R"(1)"_json,     R"("1")"_json, R"(true)"_json,
                        R"(false)"_json, R"(1.1)"_json, R"(-1.1)"_json,
                };

                for (auto it = cases.begin(); it != cases.end(); it++) {
                        auto json = *it;
                        auto yaml = to_yaml(*it);
                        REQUIRE(json == to_json(yaml));
                }
        }

        SECTION("Sequence")
        {
                const std::vector<json> cases{
                        R"([1])"_json,         R"([1,1])"_json,
                        R"(["1","1"])"_json,   R"([true])"_json,
                        R"([true,true])"_json, R"([1.1])"_json,
                        R"([1.1,1.1])"_json,   R"([-1.1])"_json,
                        R"([-1.1,-1.1])"_json, R"([-1.1,[-1.1]])"_json,
                };

                for (auto it = cases.begin(); it != cases.end(); it++) {
                        auto json = *it;
                        auto yaml = to_yaml(*it);
                        REQUIRE(json == to_json(yaml));
                }
        }

        SECTION("Map")
        {
                const std::vector<json> cases{
                        R"({"a":"b"})"_json,
                        R"([{"a":"b"}])"_json,
                        R"([{"a":1}])"_json,
                };

                for (auto it = cases.begin(); it != cases.end(); it++) {
                        auto json = *it;
                        auto yaml = to_yaml(*it);
                        REQUIRE(json == to_json(yaml));
                }
        }
}
