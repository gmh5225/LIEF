/* Copyright 2017 - 2022 R. Thomas
 * Copyright 2017 - 2022 Quarkslab
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
#include <numeric>
#include <iomanip>

#include "logging.hpp"
#include "spdlog/fmt/fmt.h"

#include "LIEF/MachO/hash.hpp"
#include "LIEF/MachO/Symbol.hpp"
#include "LIEF/MachO/RelocationFixup.hpp"
#include "LIEF/MachO/EnumToString.hpp"
#include "MachO/ChainedFixup.hpp"

namespace LIEF {
namespace MachO {

inline uint64_t unpack_target(const details::dyld_chained_ptr_64_rebase& rebase) {
  details::dyld_chained_ptr_generic64 value;
  value.rebase = rebase;
  return value.unpack_target();
}

inline uint64_t unpack_target(const details::dyld_chained_ptr_arm64e_rebase& rebase) {
  details::dyld_chained_ptr_arm64e value;
  value.rebase = rebase;
  return value.unpack_target();
}

RelocationFixup::~RelocationFixup() {
  switch (rtypes_) {
    case REBASE_TYPES::ARM64E_REBASE:      delete arm64_rebase_;      break;
    case REBASE_TYPES::ARM64E_AUTH_REBASE: delete arm64_auth_rebase_; break;
    case REBASE_TYPES::PTR64_REBASE:       delete p64_rebase_;        break;
    case REBASE_TYPES::PTR32_REBASE:       delete p32_rebase_;        break;
    case REBASE_TYPES::UNKNOWN: {}
  }
}

RelocationFixup::RelocationFixup(DYLD_CHAINED_PTR_FORMAT fmt, uint64_t imagebase) :
  ptr_fmt_{fmt},
  imagebase_{imagebase}
{}

RelocationFixup& RelocationFixup::operator=(const RelocationFixup& other) {
  if (this == &other) {
    return *this;
  }
  ptr_fmt_   = other.ptr_fmt_;
  imagebase_ = other.imagebase_;
  offset_    = other.offset_;
  rtypes_    = other.rtypes_;

  switch (rtypes_) {
    case REBASE_TYPES::ARM64E_REBASE:      arm64_rebase_      = new details::dyld_chained_ptr_arm64e_rebase{*other.arm64_rebase_}; break;
    case REBASE_TYPES::ARM64E_AUTH_REBASE: arm64_auth_rebase_ = new details::dyld_chained_ptr_arm64e_auth_rebase{*other.arm64_auth_rebase_}; break;
    case REBASE_TYPES::PTR64_REBASE:       p64_rebase_        = new details::dyld_chained_ptr_64_rebase{*other.p64_rebase_}; break;
    case REBASE_TYPES::PTR32_REBASE:       p32_rebase_        = new details::dyld_chained_ptr_32_rebase{*other.p32_rebase_}; break;
    case REBASE_TYPES::UNKNOWN: {}
  }
  return *this;
}

RelocationFixup::RelocationFixup(const RelocationFixup& other) :
  Relocation(other),
  ptr_fmt_{other.ptr_fmt_},
  imagebase_{other.imagebase_},
  offset_{other.offset_},
  rtypes_{other.rtypes_}
{
  switch (rtypes_) {
    case REBASE_TYPES::ARM64E_REBASE:      arm64_rebase_      = new details::dyld_chained_ptr_arm64e_rebase{*other.arm64_rebase_}; break;
    case REBASE_TYPES::ARM64E_AUTH_REBASE: arm64_auth_rebase_ = new details::dyld_chained_ptr_arm64e_auth_rebase{*other.arm64_auth_rebase_}; break;
    case REBASE_TYPES::PTR64_REBASE:       p64_rebase_        = new details::dyld_chained_ptr_64_rebase{*other.p64_rebase_}; break;
    case REBASE_TYPES::PTR32_REBASE:       p32_rebase_        = new details::dyld_chained_ptr_32_rebase{*other.p32_rebase_}; break;
    case REBASE_TYPES::UNKNOWN: {}
  }
}

RelocationFixup& RelocationFixup::operator=(RelocationFixup&&) = default;
RelocationFixup::RelocationFixup(RelocationFixup&&) = default;

bool RelocationFixup::is_pc_relative() const {
  return false;
}


Relocation* RelocationFixup::clone() const {
  return new RelocationFixup(*this);
}


RELOCATION_ORIGINS RelocationFixup::origin() const {
  return RELOCATION_ORIGINS::ORIGIN_CHAINED_FIXUPS;
}

void RelocationFixup::pc_relative(bool) {}

void RelocationFixup::accept(Visitor& visitor) const {
  visitor.visit(*this);
}


bool RelocationFixup::operator==(const RelocationFixup& rhs) const {
  if (this == &rhs) {
    return true;
  }
  size_t hash_lhs = Hash::hash(*this);
  size_t hash_rhs = Hash::hash(rhs);
  return hash_lhs == hash_rhs;
}

bool RelocationFixup::operator!=(const RelocationFixup& rhs) const {
  return !(*this == rhs);
}


bool RelocationFixup::classof(const Relocation& r) {
  return r.origin() == RELOCATION_ORIGINS::ORIGIN_CHAINED_FIXUPS;
}


std::ostream& RelocationFixup::print(std::ostream& os) const {
  os << fmt::format("0x{:08x}: 0x{:08x}", address(), target());
  if (const Symbol* sym = symbol()) {
    os << fmt::format("({})", sym->name());
  }
  os << "\n";
  return Relocation::print(os);
}


uint64_t RelocationFixup::target() const {
  switch (rtypes_) {
    case REBASE_TYPES::ARM64E_REBASE:
      {
        return imagebase_ + unpack_target(*arm64_rebase_);
      }

    case REBASE_TYPES::ARM64E_AUTH_REBASE:
      {
        return imagebase_ + arm64_auth_rebase_->target;
      }

    case REBASE_TYPES::PTR64_REBASE:
      {
        return imagebase_ + unpack_target(*p64_rebase_);
      }

    case REBASE_TYPES::PTR32_REBASE:
      {
        /* We tricked the parser to make sure it covers the case
         * where fixup.rebase.target > seg_info.max_valid_pointer
         */
        return imagebase_ + p32_rebase_->target;
      }

    case REBASE_TYPES::UNKNOWN:
      {
        LIEF_ERR("Can't get target: unknown rebase type");
        break;
      }
  }
  return 0;
}

void RelocationFixup::target(uint64_t target) {
  switch (rtypes_) {
    case REBASE_TYPES::ARM64E_REBASE:
      {
        uint64_t rel_target = target;
        if (rel_target >= imagebase_) {
          rel_target -= imagebase_;
        }
        pack_target(*arm64_rebase_, rel_target);
        break;
      }

    case REBASE_TYPES::ARM64E_AUTH_REBASE:
      {
        uint64_t rel_target = target;
        if (rel_target >= imagebase_) {
          rel_target -= imagebase_;
        }
        arm64_auth_rebase_->target = rel_target;
        break;
      }

    case REBASE_TYPES::PTR64_REBASE:
      {
        uint64_t rel_target = target;
        if (rel_target >= imagebase_) {
          rel_target -= imagebase_;
        }
        pack_target(*p64_rebase_, rel_target);
        break;
      }

    case REBASE_TYPES::PTR32_REBASE:
      {
        LIEF_WARN("Updating a dyld_chained_ptr_generic32 is not supported yet");
        return;
      }

    case REBASE_TYPES::UNKNOWN:
      {
        LIEF_ERR("Can't set target: unknown rebase type");
        break;
      }
  }
}

uint64_t RelocationFixup::address() const {
  return imagebase_ + offset_;
}

void RelocationFixup::address(uint64_t address)  {
  offset_ = address - imagebase_;
}

void RelocationFixup::set(const details::dyld_chained_ptr_arm64e_rebase& fixup) {
  rtypes_ = REBASE_TYPES::ARM64E_REBASE;
  arm64_rebase_ = new details::dyld_chained_ptr_arm64e_rebase(fixup);
}

void RelocationFixup::set(const details::dyld_chained_ptr_arm64e_auth_rebase& fixup) {
  rtypes_ = REBASE_TYPES::ARM64E_AUTH_REBASE;
  arm64_auth_rebase_ = new details::dyld_chained_ptr_arm64e_auth_rebase(fixup);
}

void RelocationFixup::set(const details::dyld_chained_ptr_64_rebase& fixup) {
  rtypes_ = REBASE_TYPES::PTR64_REBASE;
  p64_rebase_ = new details::dyld_chained_ptr_64_rebase(fixup);
}

void RelocationFixup::set(const details::dyld_chained_ptr_32_rebase& fixup) {
  rtypes_ = REBASE_TYPES::PTR32_REBASE;
  p32_rebase_ = new details::dyld_chained_ptr_32_rebase(fixup);
}


}
}
