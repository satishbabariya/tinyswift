// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef TINYSWIFT_TOOLCHAIN_TINY_SIL_SIL_VALUE_H_
#define TINYSWIFT_TOOLCHAIN_TINY_SIL_SIL_VALUE_H_

#include <cstdint>

#include "toolchain/tiny_sil/sil_type.h"

namespace TinySwift::TinySIL {

// A SIL value — an SSA typed value produced by an instruction or block argument.
struct SILValue {
  SILType type;
  int32_t id = -1;  // Unique value ID within the function.

  auto is_valid() const -> bool { return id >= 0; }

  auto operator==(const SILValue& other) const -> bool {
    return id == other.id;
  }
  auto operator!=(const SILValue& other) const -> bool {
    return !(*this == other);
  }

  static auto None() -> SILValue { return SILValue{.type = SILType{}, .id = -1}; }
};

}  // namespace TinySwift::TinySIL

#endif  // TINYSWIFT_TOOLCHAIN_TINY_SIL_SIL_VALUE_H_
