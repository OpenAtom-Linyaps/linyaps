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
#ifndef LIEF_DWARF_TYPE_CONST_H
#define LIEF_DWARF_TYPE_CONST_H

#include "LIEF/visibility.h"
#include "LIEF/DWARF/Type.hpp"

namespace LIEF {
namespace dwarf {
namespace types {

/// This class represents a `DW_TAG_const_type`
class LIEF_API Const : public Type {
  public:
  using Type::Type;

  static bool classof(const Type* type) {
    return type->kind() == Type::KIND::CONST_KIND;
  }

  /// The underlying type being const-ed
  const Type* underlying_type() const;

  const Type* operator->() const {
    return underlying_type();
  }

  const Type& operator*() const {
    return *underlying_type();
  }

  ~Const() override;

  protected:
  mutable std::unique_ptr<Type> underlying_;
};

}
}
}
#endif


