/* Copyright 2017 - 2024 R. Thomas
 * Copyright 2017 - 2024 Quarkslab
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef LIEF_FMT_FORMATTER
#define LIEF_FMT_FORMATTER
#include <spdlog/fmt/fmt.h>
#include <spdlog/fmt/ranges.h>

#define FMT_FORMATTER(T, F)                                          \
template <typename Char> struct fmt::formatter<T, Char> {            \
  template <typename ParseContext>                                   \
  constexpr auto parse(ParseContext& ctx) -> decltype(ctx.begin()) { \
    return ctx.begin();                                              \
  }                                                                  \
  template <typename FormatContext>                                  \
  auto format(const T& p, FormatContext& ctx) const                  \
      -> decltype(ctx.out()) {                                       \
    auto out = ctx.out();                                            \
    out = detail::write<Char>(out, F(p));                            \
    return out;                                                      \
  }                                                                  \
}

#endif

