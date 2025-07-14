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
#include "logging.hpp"
#include "LIEF/ELF/enums.hpp"
#include "LIEF/ELF/Binary.hpp"
#include "LIEF/ELF/Relocation.hpp"
#include "LIEF/ELF/Segment.hpp"
#include "LIEF/ELF/Section.hpp"
#include "LIEF/ELF/DynamicEntry.hpp"
#include "LIEF/ELF/EnumToString.hpp"

#include "ELF/Structures.hpp"
#include "ELF/DataHandler/Node.hpp"
#include "ELF/DataHandler/Handler.hpp"

#include "internal_utils.hpp"
#include "paging.hpp"

namespace LIEF {
namespace ELF {

// ===============
// ARM Relocations
// ===============
template<>
void Binary::patch_relocations<ARCH::ARM>(uint64_t from, uint64_t shift) {
  for (Relocation& relocation : relocations()) {

    if (relocation.address() >= from) {
      //shift_code(relocation.address(), shift, relocation.size() / 8);
      relocation.address(relocation.address() + shift);
    }

    const Relocation::TYPE type = relocation.type();

    switch (type) {
      case Relocation::TYPE::ARM_JUMP_SLOT:
      case Relocation::TYPE::ARM_RELATIVE:
      case Relocation::TYPE::ARM_GLOB_DAT:
      case Relocation::TYPE::ARM_IRELATIVE:
        {
          LIEF_DEBUG("Patch addend of {}", to_string(relocation));
          patch_addend<uint32_t>(relocation, from, shift);
          break;
        }

      default:
        {
          LIEF_DEBUG("Relocation {} is not patched", to_string(type));
        }
    }
  }
}


// ===================
// AARCH64 Relocations
// ===================
template<>
void Binary::patch_relocations<ARCH::AARCH64>(uint64_t from, uint64_t shift) {
  for (Relocation& relocation : relocations()) {

    if (relocation.address() >= from) {
      //shift_code(relocation.address(), shift, relocation.size() / 8);
      relocation.address(relocation.address() + shift);
    }

    const Relocation::TYPE type = relocation.type();

    switch (type) {
      case Relocation::TYPE::AARCH64_JUMP_SLOT:
      case Relocation::TYPE::AARCH64_RELATIVE:
      case Relocation::TYPE::AARCH64_GLOB_DAT:
      case Relocation::TYPE::AARCH64_IRELATIVE:
      case Relocation::TYPE::AARCH64_ABS64:
        {
          LIEF_DEBUG("Patch addend of {}", to_string(relocation));
          patch_addend<uint64_t>(relocation, from, shift);
          break;
        }

      case Relocation::TYPE::AARCH64_ABS32:
        {
          LIEF_DEBUG("Patch addend of {}", to_string(relocation));
          patch_addend<uint32_t>(relocation, from, shift);
          break;
        }

      case Relocation::TYPE::AARCH64_ABS16:
        {
          LIEF_DEBUG("Patch addend of {}", to_string(relocation));
          patch_addend<uint16_t>(relocation, from, shift);
          break;
        }

      case Relocation::TYPE::AARCH64_PREL64:
        {
          LIEF_DEBUG("Patch addend of {}", to_string(relocation));
          patch_addend<uint64_t>(relocation, from, shift);
          break;
        }

      case Relocation::TYPE::AARCH64_PREL32:
        {
          LIEF_DEBUG("Patch addend of {}", to_string(relocation));
          patch_addend<uint32_t>(relocation, from, shift);
          break;
        }

      case Relocation::TYPE::AARCH64_PREL16:
        {
          LIEF_DEBUG("Patch addend of {}", to_string(relocation));
          patch_addend<uint16_t>(relocation, from, shift);
          break;
        }

      default:
        {
          LIEF_DEBUG("Relocation {} is not patched", to_string(type));
        }
    }
  }
}

// ==================
// x86_32 Relocations
// ==================
template<>
void Binary::patch_relocations<ARCH::I386>(uint64_t from, uint64_t shift) {
  for (Relocation& relocation : relocations()) {
    if (relocation.address() >= from) {
      //shift_code(relocation.address(), shift, relocation.size() / 8);
      relocation.address(relocation.address() + shift);
    }
    const Relocation::TYPE type = relocation.type();

    switch (type) {
      case Relocation::TYPE::X86_RELATIVE:
      case Relocation::TYPE::X86_JUMP_SLOT:
      case Relocation::TYPE::X86_IRELATIVE:
      case Relocation::TYPE::X86_GLOB_DAT:
      case Relocation::TYPE::X86_32:
        {
          LIEF_DEBUG("Patch addend of {}", to_string(relocation));
          patch_addend<uint32_t>(relocation, from, shift);
          break;
        }
      case Relocation::TYPE::X86_TLS_DTPMOD32:
      case Relocation::TYPE::X86_TLS_DTPOFF32:
        {
          // Nothing to do for these relocations
          continue;
        }

      default:
        {
          LIEF_WARN("Relocation {} not supported!", to_string(type));
        }
    }
  }
}

// ==================
// x86_64 Relocations
// ==================
template<>
void Binary::patch_relocations<ARCH::X86_64>(uint64_t from, uint64_t shift) {
  for (Relocation& relocation : relocations()) {
    if (relocation.address() >= from) {
      relocation.address(relocation.address() + shift);
    }

    const Relocation::TYPE type = relocation.type();

    switch (type) {
      case Relocation::TYPE::X86_64_RELATIVE:
      case Relocation::TYPE::X86_64_IRELATIVE:
      case Relocation::TYPE::X86_64_JUMP_SLOT:
      case Relocation::TYPE::X86_64_GLOB_DAT:
      case Relocation::TYPE::X86_64_64:
        {
          LIEF_DEBUG("Patch addend of {}", to_string(relocation));
          patch_addend<uint64_t>(relocation, from, shift);
          break;
        }

      case Relocation::TYPE::X86_64_32:
        {
          LIEF_DEBUG("Patch addend of {}", to_string(relocation));
          patch_addend<uint32_t>(relocation, from, shift);
          break;
        }

      default:
        {
          LIEF_DEBUG("Relocation {} is not patched", to_string(type));
        }
    }
  }
}


// ==================
// PPC_32 Relocations
// ==================
template<>
void Binary::patch_relocations<ARCH::PPC>(uint64_t from, uint64_t shift) {
  for (Relocation& relocation : relocations()) {
    if (relocation.address() >= from) {
      relocation.address(relocation.address() + shift);
    }

    const Relocation::TYPE type = relocation.type();

    switch (type) {
      case Relocation::TYPE::PPC_RELATIVE:
        {
          LIEF_DEBUG("Patch addend of {}", to_string(relocation));
          patch_addend<uint32_t>(relocation, from, shift);
          break;
        }

      default:
        {
          LIEF_DEBUG("Relocation {} is not patched", to_string(type));
        }
    }
  }
}

// ==================
// RISCV Relocations
// ==================
template<>
void Binary::patch_relocations<ARCH::RISCV>(uint64_t from, uint64_t shift) {
  for (Relocation& relocation : relocations()) {
    if (relocation.address() >= from) {
      relocation.address(relocation.address() + shift);
    }

    const Relocation::TYPE type = relocation.type();
    const bool is64 = this->type_ == Header::CLASS::ELF64;

    switch (type) {
      case Relocation::TYPE::RISCV_32:
      case Relocation::TYPE::RISCV_TLS_DTPREL32:
      case Relocation::TYPE::RISCV_TLS_TPREL32:
        {
          LIEF_DEBUG("Patch addend of {}", to_string(relocation));
          patch_addend<uint32_t>(relocation, from, shift);
          break;
        }

      case Relocation::TYPE::RISCV_64:
      case Relocation::TYPE::RISCV_TLS_DTPMOD64:
      case Relocation::TYPE::RISCV_TLS_DTPREL64:
      case Relocation::TYPE::RISCV_TLS_TPREL64:
        {
          LIEF_DEBUG("Patch addend of {}", to_string(relocation));
          patch_addend<uint64_t>(relocation, from, shift);
          break;
        }

      case Relocation::TYPE::RISCV_RELATIVE:
      case Relocation::TYPE::RISCV_IRELATIVE:
        {
          LIEF_DEBUG("Patch addend of {}", to_string(relocation));
          is64 ? patch_addend<uint64_t>(relocation, from, shift) :
                 patch_addend<uint32_t>(relocation, from, shift);
          break;
        }

      default:
        {
          LIEF_DEBUG("Relocation {} is not patched", to_string(type));
        }
    }
  }
}

template<class T>
void Binary::patch_addend(Relocation& relocation, uint64_t from, uint64_t shift) {
  if (static_cast<uint64_t>(relocation.addend()) >= from) {
    relocation.addend(relocation.addend() + shift);
  }

  const uint64_t address = relocation.address();
  LIEF_DEBUG("Patch addend relocation at address: 0x{:x}", address);
  Segment* segment = segment_from_virtual_address(address);
  if (segment == nullptr) {
    LIEF_ERR("Can't find segment with the virtual address 0x{:x}", address);
  }

  auto res_offset = virtual_address_to_offset(address);
  if (!res_offset) {
    LIEF_ERR("Can't convert the virtual address 0x{:06x} into an offset", address);
    return;
  }
  const uint64_t relative_offset = *res_offset - segment->file_offset();

  const size_t segment_size = segment->get_content_size();

  if (segment_size == 0) {
    LIEF_WARN("Segment is empty nothing to do");
    return;
  }

  if (relative_offset >= segment_size || (relative_offset + sizeof(T)) > segment_size) {
    LIEF_DEBUG("Offset out of bound for relocation: {}", to_string(relocation));
    return;
  }

  auto value = segment->get_content_value<T>(relative_offset);

  if (value >= from) {
    value += shift;
  }

  segment->set_content_value(relative_offset, value);
}


// ========
// ET_EXEC: non-pie executable
//
// For non-pie executable, we move the phdr table at the end of the
// binary and we extend the closest PT_LOAD segment to the end of the
// binary so that it loads the new location of the phdr
//
// The ELF format requires the following relationship between
// segment's VA and segment's offset:
//
// segment_va % pagesize() == segment_offset % pagesize()
//
// It implies that we usually find "cave" between segments
// that could can be large enough to insert our new phdr table.
// To do so, we would just need to extend the PT_LOAD segment associated
// with the caving.
template<>
Segment* Binary::add_segment<Header::FILE_TYPE::EXEC>(const Segment& segment, uint64_t base) {
  Header& header = this->header();
  const uint64_t new_phdr_offset = relocate_phdr_table_auto();

  if (new_phdr_offset == 0) {
    LIEF_ERR("We can't relocate the PHDR table for this binary.");
    return nullptr;
  }

  if (phdr_reloc_info_.nb_segments == 0) {
    LIEF_ERR("The segment table is full. We can't add segment");
    return nullptr;
  }

  // Add the segment itself
  // ====================================================================
  header.numberof_segments(header.numberof_segments() + 1);
  span<const uint8_t> content_ref = segment.content();
  std::vector<uint8_t> content{content_ref.data(), std::end(content_ref)};
  auto new_segment = std::make_unique<Segment>(segment);

  uint64_t last_offset_sections = last_offset_section();
  uint64_t last_offset_segments = last_offset_segment();


  uint64_t last_offset = std::max<uint64_t>(last_offset_sections, last_offset_segments);

  const auto psize = static_cast<uint64_t>(get_pagesize(*this));
  const uint64_t last_offset_aligned = align(last_offset, psize);
  new_segment->file_offset(last_offset_aligned);

  if (segment.virtual_address() == 0) {
    new_segment->virtual_address(base + last_offset_aligned);
  }

  new_segment->physical_address(new_segment->virtual_address());

  uint64_t segmentsize = align(content.size(), psize);
  content.resize(segmentsize, 0);

  new_segment->handler_size_ = content.size();
  new_segment->physical_size(segmentsize);
  new_segment->virtual_size(segmentsize);

  if (new_segment->alignment() == 0) {
    new_segment->alignment(psize);
  }
  new_segment->datahandler_ = datahandler_.get();

  DataHandler::Node new_node{new_segment->file_offset(), new_segment->physical_size(),
                             DataHandler::Node::SEGMENT};
  datahandler_->add(new_node);
  auto alloc = datahandler_->make_hole(last_offset_aligned, new_segment->physical_size());
  if (!alloc) {
    LIEF_ERR("Allocation failed");
    return nullptr;
  }
  new_segment->content(content);

  if (header.section_headers_offset() <= new_segment->file_offset() + new_segment->physical_size()) {
    header.section_headers_offset(new_segment->file_offset() + new_segment->physical_size());
  }

  const auto it_new_segment_place = std::find_if(segments_.rbegin(), segments_.rend(),
      [&new_segment] (const std::unique_ptr<Segment>& s) { return s->type() == new_segment->type(); });

  Segment* seg_ptr = new_segment.get();
  if (it_new_segment_place == segments_.rend()) {
    segments_.push_back(std::move(new_segment));
  } else {
    const size_t idx = std::distance(std::begin(segments_), it_new_segment_place.base());
    segments_.insert(std::begin(segments_) + idx, std::move(new_segment));
  }
  phdr_reloc_info_.nb_segments--;
  return seg_ptr;
}

// =======================
// ET_DYN (PIE/Libraries)
// =======================
template<>
Segment* Binary::add_segment<Header::FILE_TYPE::DYN>(const Segment& segment, uint64_t base) {
  const auto psize = static_cast<uint64_t>(get_pagesize(*this));

  /*const uint64_t new_phdr_offset = */ relocate_phdr_table_auto();

  span<const uint8_t> content_ref = segment.content();
  std::vector<uint8_t> content{content_ref.data(), std::end(content_ref)};

  auto new_segment = std::make_unique<Segment>(segment);
  new_segment->datahandler_ = datahandler_.get();

  DataHandler::Node new_node{new_segment->file_offset(), new_segment->physical_size(),
                             DataHandler::Node::SEGMENT};
  datahandler_->add(new_node);

  const uint64_t last_offset_sections = last_offset_section();
  const uint64_t last_offset_segments = last_offset_segment();
  const uint64_t last_offset          = std::max<uint64_t>(last_offset_sections, last_offset_segments);
  const uint64_t last_offset_aligned  = align(last_offset, psize);

  new_segment->file_offset(last_offset_aligned);
  new_segment->virtual_address(new_segment->file_offset() + base);
  new_segment->physical_address(new_segment->virtual_address());

  uint64_t segmentsize = align(content.size(), 0x10);
  //uint64_t segmentsize = content.size();
  new_segment->handler_size_ = content.size();
  new_segment->physical_size(segmentsize);
  new_segment->virtual_size(segmentsize);

  if (new_segment->alignment() == 0) {
    new_segment->alignment(psize);
  }

  // Patch SHDR
  Header& header = this->header();
  const uint64_t new_section_hdr_offset = new_segment->file_offset() + new_segment->physical_size();
  header.section_headers_offset(new_section_hdr_offset);

  auto alloc = datahandler_->make_hole(last_offset_aligned, new_segment->physical_size());

  if (!alloc) {
    LIEF_ERR("Allocation failed");
    return nullptr;
  }

  new_segment->content(content);

  header.numberof_segments(header.numberof_segments() + 1);

  const auto& it_new_segment_place = std::find_if(
      segments_.rbegin(), segments_.rend(),
      [&new_segment] (const std::unique_ptr<Segment>& s) {
        return s->type() == new_segment->type();
      });
  Segment* seg_ptr = new_segment.get();
  if (it_new_segment_place == segments_.rend()) {
    segments_.push_back(std::move(new_segment));
  } else {
    const size_t idx = std::distance(std::begin(segments_), it_new_segment_place.base());
    segments_.insert(std::begin(segments_) + idx, std::move(new_segment));
  }

  return seg_ptr;
}


// =======================
// Extend PT_LOAD
// =======================
template<>
Segment* Binary::extend_segment<Segment::TYPE::LOAD>(const Segment& segment, uint64_t size) {

  const auto it_segment = std::find_if(std::begin(segments_), std::end(segments_),
                                       [&segment] (const std::unique_ptr<Segment>& s) {
                                          return *s == segment;
                                       });

  if (it_segment == std::end(segments_)) {
    LIEF_ERR("Unable to find the segment in the current binary");
    return nullptr;
  }

  std::unique_ptr<Segment>& segment_to_extend = *it_segment;


  uint64_t from_offset  = segment_to_extend->file_offset() + segment_to_extend->physical_size();
  uint64_t from_address = segment_to_extend->virtual_address() + segment_to_extend->virtual_size();
  uint64_t shift        = size;

  auto alloc = datahandler_->make_hole(
      segment_to_extend->file_offset() + segment_to_extend->physical_size(),
      size);

  if (!alloc) {
    LIEF_ERR("Allocation failed");
    return nullptr;
  }

  shift_sections(from_offset, shift);
  shift_segments(from_offset, shift);

  // Shift
  segment_to_extend->physical_size(segment_to_extend->physical_size() + size);
  segment_to_extend->virtual_size(segment_to_extend->virtual_size() + size);

  span<const uint8_t> content_ref = segment_to_extend->content();
  std::vector<uint8_t> segment_content{content_ref.data(), std::end(content_ref)};

  segment_content.resize(segment_to_extend->physical_size(), 0);
  segment_to_extend->content(segment_content);

  // Patches
  header().section_headers_offset(header().section_headers_offset() + shift);

  shift_dynamic_entries(from_address, shift);
  shift_symbols(from_address, shift);
  shift_relocations(from_address, shift);

  if (type() == Header::CLASS::ELF32) {
    fix_got_entries<details::ELF32>(from_address, shift);
  } else {
    fix_got_entries<details::ELF64>(from_address, shift);
  }

  if (header().entrypoint() >= from_address) {
    header().entrypoint(header().entrypoint() + shift);
  }

  return segment_to_extend.get();
}


template<>
Section* Binary::add_section<true>(const Section& section) {
  LIEF_DEBUG("Adding section '{}' as LOADED", section.name());
  // Create a Segment:
  Segment new_segment;
  span<const uint8_t> content_ref = section.content();
  new_segment.content({std::begin(content_ref), std::end(content_ref)});
  new_segment.type(Segment::TYPE::LOAD);

  new_segment.virtual_address(section.virtual_address());
  new_segment.physical_address(section.virtual_address());

  new_segment.physical_size(section.size());
  new_segment.file_offset(section.offset());
  new_segment.alignment(section.alignment());

  new_segment.add(Segment::FLAGS::R);

  if (section.has(Section::FLAGS::WRITE)) {
    new_segment.add(Segment::FLAGS::W);
  }

  if (section.has(Section::FLAGS::EXECINSTR)) {
    new_segment.add(Segment::FLAGS::X);
  }

  Segment* segment_added = add(new_segment);
  if (segment_added == nullptr) {
    LIEF_ERR("Can't add a LOAD segment of the section");
    return nullptr;
  }

  LIEF_DEBUG("Segment associated: {}@0x{:x}",
             to_string(segment_added->type()), segment_added->virtual_address());

  auto new_section = std::make_unique<Section>(section);
  new_section->datahandler_ = datahandler_.get();

  DataHandler::Node new_node{new_section->file_offset(), new_section->size(),
                             DataHandler::Node::SECTION};
  datahandler_->add(new_node);

  new_section->virtual_address(segment_added->virtual_address());
  new_section->size(segment_added->physical_size());
  new_section->offset(segment_added->file_offset());
  new_section->original_size_ = segment_added->physical_size();

  new_section->segments_.push_back(segment_added);
  segment_added->sections_.push_back(new_section.get());

  header().numberof_sections(header().numberof_sections() + 1);

  Section* sec_ptr = new_section.get();
  sections_.push_back(std::move(new_section));
  return sec_ptr;
}

// Add a non-loaded section
template<>
Section* Binary::add_section<false>(const Section& section) {

  auto new_section = std::make_unique<Section>(section);
  new_section->datahandler_ = datahandler_.get();

  DataHandler::Node new_node{new_section->file_offset(), new_section->size(),
                             DataHandler::Node::SECTION};
  datahandler_->add(new_node);

  const uint64_t last_offset_sections = last_offset_section();
  const uint64_t last_offset_segments = last_offset_segment();
  const uint64_t last_offset          = std::max<uint64_t>(last_offset_sections, last_offset_segments);

  auto alloc = datahandler_->make_hole(last_offset, section.size());
  if (!alloc) {
    LIEF_ERR("Allocation failed");
    return nullptr;
  }

  new_section->offset(last_offset);
  new_section->size(section.size());

  // Copy original content in the data handler
  span<const uint8_t> content_ref = section.content();
  new_section->content({std::begin(content_ref), std::end(content_ref)});

  header().numberof_sections(header().numberof_sections() + 1);

  Header& header = this->header();
  const uint64_t new_section_hdr_offset = new_section->offset() + new_section->size();
  header.section_headers_offset(new_section_hdr_offset);
  Section* sec_ptr = new_section.get();
  sections_.push_back(std::move(new_section));
  return sec_ptr;
}

template<class ELF_T>
void Binary::fix_got_entries(uint64_t from, uint64_t shift) {
  using ptr_t = typename ELF_T::Elf_Addr;

  DynamicEntry* dt_pltgot = get(DynamicEntry::TAG::PLTGOT);
  if (dt_pltgot == nullptr) {
    return;
  }
  const uint64_t addr = dt_pltgot->value();
  span<const uint8_t> content = get_content_from_virtual_address(addr, 3 * sizeof(ptr_t));
  std::vector<uint8_t> content_vec(content.begin(), content.end());
  if (content.size() != 3 * sizeof(ptr_t)) {
    LIEF_ERR("Cant't read got entries!");
    return;
  }

  auto got = reinterpret_cast<ptr_t*>(content_vec.data());
  if (got[0] > 0 && got[0] > from) { // Offset to the dynamic section
    got[0] += shift;
  }

  if (got[1] > 0 && got[1] > from) { // Prelinked value (unlikely?)
    got[1] += shift;
  }
  patch_address(addr, content_vec);
}

}
}
