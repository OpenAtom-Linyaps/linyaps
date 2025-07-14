/* Copyright 2022 - 2024 R. Thomas
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
#ifndef LIEF_PDB_TYPE_ENUM_H
#define LIEF_PDB_TYPE_ENUM_H

#include "LIEF/visibility.h"
#include "LIEF/PDB/Type.hpp"

namespace LIEF {
namespace pdb {
namespace types {

/// This class represents a `LF_ENUM` PDB type
class LIEF_API Enum : public Type {
  public:
  using Type::Type;

  static bool classof(const Type* type) {
    return type->kind() == Type::KIND::ENUM;
  }

  ~Enum() override;
};

}
}
}
#endif


