// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "toolchain/sem_ir/type.h"

#include "toolchain/sem_ir/file.h"

namespace TinySwift::SemIR {

auto TypeStore::GetConstantId(TypeId type_id) const -> ConstantId {
  if (!type_id.has_value()) {
    return ConstantId::NotConstant;
  }
  return type_id.AsConstantId();
}

auto TypeStore::GetTypeIdForTypeConstantId(ConstantId constant_id) const
    -> TypeId {
  return TypeId::ForTypeConstant(constant_id);
}

auto TypeStore::GetTypeIdForTypeInstId(InstId inst_id) const -> TypeId {
  auto constant_id = file_->constant_values().Get(inst_id);
  return TypeId::ForTypeConstant(constant_id);
}

auto TypeStore::GetTypeIdForTypeInstId(TypeInstId inst_id) const -> TypeId {
  auto constant_id = file_->constant_values().Get(inst_id);
  return TypeId::ForTypeConstant(constant_id);
}

auto TypeStore::GetAsTypeInstId(InstId inst_id) const -> TypeInstId {
  return TypeInstId::UnsafeMake(inst_id);
}

auto TypeStore::GetTypeInstId(TypeId type_id) const -> TypeInstId {
  return TypeInstId::UnsafeMake(
      file_->constant_values().GetInstId(GetConstantId(type_id)));
}

auto TypeStore::GetAsInst(TypeId type_id) const -> Inst {
  return file_->insts().Get(GetTypeInstId(type_id));
}

auto TypeStore::GetUnattachedType(TypeId type_id) const -> TypeId {
  return TypeId::ForTypeConstant(
      file_->constant_values().GetUnattachedConstant(type_id.AsConstantId()));
}

// Template method implementations that need File to be complete.
template <typename InstT>
auto TypeStore::Is(TypeId type_id) const -> bool {
  return GetAsInst(type_id).Is<InstT>();
}

template <typename InstT>
auto TypeStore::GetAs(TypeId type_id) const -> InstT {
  return GetAsInst(type_id).As<InstT>();
}

template <typename InstT>
auto TypeStore::TryGetAs(TypeId type_id) const -> std::optional<InstT> {
  return GetAsInst(type_id).TryAs<InstT>();
}

}  // namespace TinySwift::SemIR
