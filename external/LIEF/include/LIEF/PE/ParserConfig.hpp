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
#ifndef LIEF_PE_PARSER_CONFIG_H
#define LIEF_PE_PARSER_CONFIG_H
#include "LIEF/visibility.h"

namespace LIEF {
namespace PE {

/// This structure is used to tweak the PE Parser (PE::Parser)
struct LIEF_API ParserConfig {
  static ParserConfig all() {
    static const ParserConfig DEFAULT;
    return DEFAULT;
  }

  bool parse_signature = true; ///< Parse PE Authenticode signature
  bool parse_exports   = true; ///< Parse PE Exports Directory
  bool parse_imports   = true; ///< Parse PE Import Directory
  bool parse_rsrc      = true; ///< Parse PE resources tree
  bool parse_reloc     = true; ///< Parse PE relocations
};

}
}
#endif
