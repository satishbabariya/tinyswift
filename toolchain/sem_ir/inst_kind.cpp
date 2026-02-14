// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "toolchain/sem_ir/inst_kind.h"

#include "toolchain/sem_ir/typed_insts.h"

namespace TinySwift::SemIR {

TINYSWIFT_DEFINE_ENUM_CLASS_NAMES(InstKind) {
#define TINYSWIFT_SEM_IR_INST_KIND(Name) TINYSWIFT_ENUM_CLASS_NAME_STRING(Name)
#include "toolchain/sem_ir/inst_kind.def"
};

auto InstKind::definition_info(InstKind inst_kind) -> const DefinitionInfo& {
  static constexpr InstKind::DefinitionInfo DefinitionInfos[] = {
#define TINYSWIFT_SEM_IR_INST_KIND(Name) SemIR::Name::Kind.info_,
#include "toolchain/sem_ir/inst_kind.def"
  };
  return DefinitionInfos[inst_kind.AsInt()];
}

auto InstKind::has_type() const -> bool {
  static constexpr bool Table[] = {
#define TINYSWIFT_SEM_IR_INST_KIND(Name) Internal::HasTypeIdMember<SemIR::Name>,
#include "toolchain/sem_ir/inst_kind.def"
  };
  return Table[AsInt()];
}

}  // namespace TinySwift::SemIR
