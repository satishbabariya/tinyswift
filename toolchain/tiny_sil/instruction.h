// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef TINYSWIFT_TOOLCHAIN_TINY_SIL_INSTRUCTION_H_
#define TINYSWIFT_TOOLCHAIN_TINY_SIL_INSTRUCTION_H_

#include <cstdint>
#include <string>
#include <variant>

#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "toolchain/sem_ir/ids.h"
#include "toolchain/tiny_sil/sil_value.h"

namespace TinySwift::TinySIL {

// SIL instruction kinds.
enum class SILInstKind : int32_t {
  // Memory
  AllocStack,
  DeallocStack,
  AllocBox,
  Load,
  Store,

  // Values
  IntegerLiteral,
  FloatLiteral,
  StringLiteral,
  StructInst,
  TupleInst,
  EnumInst,

  // Access
  StructExtract,
  StructElementAddr,
  TupleExtract,
  TupleElementAddr,

  // Functions
  FunctionRef,
  Apply,
  PartialApply,
  ReturnInst,

  // Control flow
  Branch,
  CondBranch,
  SwitchEnum,
  Unreachable,

  // Conversions
  ConvertFunction,
  Upcast,
  UncheckedEnumData,

  // Ownership
  CopyValue,
  DestroyValue,
  BeginBorrow,
  EndBorrow,

  // Debug
  DebugValue,

  // Builtins
  BuiltinInst,
};

// A SIL instruction.
struct SILInstruction {
  SILInstKind kind;
  SILValue result;  // The value produced by this instruction (if any).

  // Operands — up to 3 direct operands plus an optional operand list.
  SILValue operands[3] = {};
  int32_t num_operands = 0;

  // Extra data depending on kind.
  int64_t literal_value = 0;          // For IntegerLiteral.
  double float_literal_value = 0.0;   // For FloatLiteral.
  std::string string_literal_value;   // For StringLiteral.
  std::string function_name;          // For FunctionRef.
  std::string builtin_name;           // For BuiltinInst.

  int32_t target_block = -1;          // For Branch.
  int32_t true_block = -1;            // For CondBranch.
  int32_t false_block = -1;           // For CondBranch.

  int32_t element_index = -1;         // For StructExtract, TupleExtract, etc.

  SILType alloc_type;                 // For AllocStack.

  // Operand list for variable-length operands (Apply args, Struct fields).
  llvm::SmallVector<SILValue> operand_list;

  // Block argument lists for branches.
  llvm::SmallVector<SILValue> branch_args;

  // Ownership annotation.
  Ownership ownership = Ownership::None;

  // M119: Source location for debug info (DWARF line-level stepping).
  SemIR::LocId loc_id = SemIR::LocId::None;

  auto getOperand(int index) const -> SILValue { return operands[index]; }
  auto setOperand(int index, SILValue val) -> void {
    operands[index] = val;
    if (index >= num_operands) {
      num_operands = index + 1;
    }
  }
};

}  // namespace TinySwift::TinySIL

#endif  // TINYSWIFT_TOOLCHAIN_TINY_SIL_INSTRUCTION_H_
