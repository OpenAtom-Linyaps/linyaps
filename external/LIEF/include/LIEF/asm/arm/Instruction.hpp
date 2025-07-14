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
#ifndef LIEF_ASM_ARM_INST_H
#define LIEF_ASM_ARM_INST_H
#include "LIEF/visibility.h"

#include "LIEF/asm/Instruction.hpp"
#include "LIEF/asm/arm/opcodes.hpp"

namespace LIEF {
namespace assembly {
/// ARM architecture-related namespace
namespace arm {

/// This class represents an ARM/Thumb instruction
class LIEF_API Instruction : public assembly::Instruction {
  public:
  using assembly::Instruction::Instruction;

  /// The instruction opcode as defined in LLVM
  OPCODE opcode() const;

  /// True if `inst` is an **effective** instance of arm::Instruction
  static bool classof(const assembly::Instruction* inst);

  ~Instruction() override = default;
};
}
}
}
#endif
