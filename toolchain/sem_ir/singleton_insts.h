// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef TINYSWIFT_TOOLCHAIN_SEM_IR_SINGLETON_INSTS_H_
#define TINYSWIFT_TOOLCHAIN_SEM_IR_SINGLETON_INSTS_H_

#include <array>

#include "toolchain/sem_ir/ids.h"
#include "toolchain/sem_ir/inst_kind.h"

namespace TinySwift::SemIR {

// The list of singleton instruction kinds. Singleton instructions are created
// once in the File constructor and are always available. Each singleton
// instruction has a fixed InstId equal to its index in this array.
inline constexpr std::array SingletonInstKinds = {
    InstKind::ErrorInst,
    InstKind::BoolType,
    InstKind::IntLiteralType,
    InstKind::NamespaceType,
    InstKind::TypeType,
};

// Returns whether the given instruction kind is a singleton.
constexpr auto IsSingletonInstKind(InstKind kind) -> bool {
  for (auto singleton_kind : SingletonInstKinds) {
    if (kind == singleton_kind) {
      return true;
    }
  }
  return false;
}

// Returns whether the given instruction ID refers to a singleton instruction.
constexpr auto IsSingletonInstId(InstId id) -> bool {
  return id.has_value() && id.index >= 0 &&
         static_cast<size_t>(id.index) < SingletonInstKinds.size();
}

// Returns the TypeInstId for a singleton type instruction kind. The TypeInstId
// is derived from the position of the kind in SingletonInstKinds.
template <const auto& Kind>
constexpr auto MakeSingletonTypeInstId() -> TypeInstId {
  for (size_t i = 0; i < SingletonInstKinds.size(); ++i) {
    if (SingletonInstKinds[i] == Kind) {
      return TypeInstId::UnsafeMake(InstId(static_cast<int32_t>(i)));
    }
  }
  return TypeInstId::None;
}

}  // namespace TinySwift::SemIR

#endif  // TINYSWIFT_TOOLCHAIN_SEM_IR_SINGLETON_INSTS_H_
