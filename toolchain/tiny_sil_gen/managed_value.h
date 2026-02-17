// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef TINYSWIFT_TOOLCHAIN_TINY_SIL_GEN_MANAGED_VALUE_H_
#define TINYSWIFT_TOOLCHAIN_TINY_SIL_GEN_MANAGED_VALUE_H_

#include "toolchain/tiny_sil/sil_value.h"

namespace TinySwift::TinySILGen {

// A SILValue paired with an optional cleanup (destroy).
// When the value goes out of scope, the cleanup fires.
// This tracks ARC without manual retain/release everywhere.
class ManagedValue {
 public:
  ManagedValue() = default;
  explicit ManagedValue(TinySIL::SILValue value, bool has_cleanup = false)
      : value_(value), has_cleanup_(has_cleanup) {}

  auto getValue() const -> TinySIL::SILValue { return value_; }
  auto hasCleanup() const -> bool { return has_cleanup_; }
  auto isValid() const -> bool { return value_.is_valid(); }

  static auto forUnmanaged(TinySIL::SILValue value) -> ManagedValue {
    return ManagedValue(value, /*has_cleanup=*/false);
  }

  static auto invalid() -> ManagedValue { return ManagedValue(); }

 private:
  TinySIL::SILValue value_;
  bool has_cleanup_ = false;
};

}  // namespace TinySwift::TinySILGen

#endif  // TINYSWIFT_TOOLCHAIN_TINY_SIL_GEN_MANAGED_VALUE_H_
