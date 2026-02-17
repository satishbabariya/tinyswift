// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef TINYSWIFT_TOOLCHAIN_TINY_SIL_SIL_TYPE_H_
#define TINYSWIFT_TOOLCHAIN_TINY_SIL_SIL_TYPE_H_

#include <cstdint>
#include <string>

#include "llvm/ADT/StringRef.h"

namespace TinySwift::TinySIL {

// Ownership convention for SIL values.
enum class Ownership : int8_t {
  None,        // No ownership tracking (trivial types like Int, Bool).
  Owned,       // The value is owned; the holder must destroy it.
  Guaranteed,  // The value is borrowed; must not be destroyed.
  Unowned,     // An unowned reference (unsafe).
};

// A SIL type, representing the type of a SIL value.
// This wraps a type identifier from the original SemIR type system.
struct SILType {
  int32_t type_index = -1;  // Index into the SemIR type store.
  bool is_address = false;  // True if this is an address type ($*T).

  auto is_valid() const -> bool { return type_index >= 0; }

  auto operator==(const SILType& other) const -> bool {
    return type_index == other.type_index && is_address == other.is_address;
  }
  auto operator!=(const SILType& other) const -> bool {
    return !(*this == other);
  }

  // Creates an address type for this object type.
  auto getAddressType() const -> SILType {
    return SILType{.type_index = type_index, .is_address = true};
  }

  // Creates an object type from this address type.
  auto getObjectType() const -> SILType {
    return SILType{.type_index = type_index, .is_address = false};
  }
};

// A SIL function type, representing the signature of a function in SIL.
struct SILFunctionType {
  SILType return_type;
  llvm::SmallVector<SILType> param_types;
  llvm::SmallVector<Ownership> param_conventions;
  Ownership return_convention = Ownership::Owned;
  bool is_void_return = false;
};

}  // namespace TinySwift::TinySIL

#endif  // TINYSWIFT_TOOLCHAIN_TINY_SIL_SIL_TYPE_H_
