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
                        R"(null)"_json,
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
                        R"([])"_json,
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
                        R"({})"_json,
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

        SECTION("Nested empty containers survive serialization")
        {
                const std::vector<json> cases{
                        R"([[],{}])"_json,
                        R"({"sources":[],"metadata":{}})"_json,
                };

                for (const auto &json : cases) {
                        INFO(json.dump());
                        auto yaml = to_yaml(json);
                        REQUIRE(json == to_json(yaml));
                        REQUIRE(json == to_json(YAML::Load(YAML::Dump(yaml))));
                }
        }
}
