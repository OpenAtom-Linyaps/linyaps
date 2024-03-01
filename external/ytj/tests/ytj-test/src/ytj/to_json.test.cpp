#include "ytj/find_catch2.hpp"
#include "ytj/ytj.hpp"

using nlohmann::json;
using ytj::to_json;

TEST_CASE("to_json")
{
        SECTION("Scalar")
        {
                const std::vector<std::pair<std::string, json>> cases{
                        { R"(1)", R"(1)"_json },
                        { R"("1")", R"("1")"_json },
                        { R"(true)", R"(true)"_json },
                        { R"(false)", R"(false)"_json },
                        { R"(1.1)", R"(1.1)"_json },
                        { R"(-1.1)", R"(-1.1)"_json },
                };

                for (const auto &it : cases) {
                        auto yaml = it.first;
                        auto json = it.second;
                        REQUIRE(to_json(YAML::Load(yaml)) == json);
                }
        }

        SECTION("Sequence")
        {
                const std::vector<std::pair<std::string, json>> cases{
                        { R"([1])", R"([1])"_json },
                        { R"([1,1])", R"([1,1])"_json },
                        { R"(["1","1"])", R"(["1","1"])"_json },
                        { R"([true])", R"([true])"_json },
                        { R"([true,true])", R"([true,true])"_json },
                        { R"([1.1])", R"([1.1])"_json },
                        { R"([1.1,1.1])", R"([1.1,1.1])"_json },
                        { R"([-1.1])", R"([-1.1])"_json },
                        { R"([-1.1,-1.1])", R"([-1.1,-1.1])"_json },
                        { R"([-1.1,[-1.1]])", R"([-1.1,[-1.1]])"_json },
                };

                for (const auto &it : cases) {
                        auto yaml = it.first;
                        auto json = it.second;
                        REQUIRE(to_json(YAML::Load(yaml)) == json);
                }
        }

        SECTION("Map")
        {
                const std::vector<std::pair<std::string, json>> cases{
                        { R"({a: b})", R"({"a":"b"})"_json },
                        { R"([{a: b}])", R"([{"a":"b"}])"_json },
                        { R"([{a: 1}])", R"([{"a":1}])"_json },
                };

                for (const auto &it : cases) {
                        auto yaml = it.first;
                        auto json = it.second;
                        REQUIRE(to_json(YAML::Load(yaml)) == json);
                }
        }
}
