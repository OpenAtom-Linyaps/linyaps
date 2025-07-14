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
#ifndef LIEF_ELF_NOTE_DETAILS_PROPERTIES_NEEDED_H
#define LIEF_ELF_NOTE_DETAILS_PROPERTIES_NEEDED_H

#include "LIEF/ELF/NoteDetails/NoteGnuProperty.hpp"

namespace LIEF {
namespace ELF {

class Needed : public NoteGnuProperty::Property {
  public:
  enum class NEED {
    UNKNOWN = 0,
    INDIRECT_EXTERN_ACCESS,
  };
  static bool classof(const NoteGnuProperty::Property* prop) {
    return prop->type() == NoteGnuProperty::Property::TYPE::NEEDED;
  }

  ~Needed() override = default;
};
}
}

#endif
