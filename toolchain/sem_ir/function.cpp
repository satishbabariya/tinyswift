// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "toolchain/sem_ir/function.h"

#include "toolchain/sem_ir/file.h"

// TODO: Implement your language's function-related operations here.
// See TinySwift compiler for reference implementation patterns.

namespace TinySwift::SemIR {

auto GetCallee(const File& sem_ir, InstId callee_id,
               SpecificId /*caller_specific_id*/) -> Callee {
  // Get the constant value of the callee instruction.
  auto val_id = sem_ir.constant_values().GetConstantInstId(callee_id);
  if (!val_id.has_value()) {
    return CalleeNonFunction();
  }

  // Identify the function we're calling by its type.
  auto fn_type_inst =
      sem_ir.types().GetAsInst(sem_ir.insts().Get(val_id).type_id());

  if (fn_type_inst.Is<ErrorInst>()) {
    return CalleeError();
  }

  auto fn_type = fn_type_inst.TryAs<FunctionType>();
  if (!fn_type) {
    return CalleeNonFunction();
  }

  return CalleeFunction{.function_id = fn_type->function_id,
                        .enclosing_specific_id = fn_type->specific_id,
                        .resolved_specific_id = SpecificId::None,
                        .self_id = InstId::None};
}

auto GetCalleeAsFunction(const File& sem_ir, InstId callee_id,
                         SpecificId caller_specific_id) -> CalleeFunction {
  return std::get<CalleeFunction>(
      GetCallee(sem_ir, callee_id, caller_specific_id));
}

auto Function::GetDeclaredReturnType(const File& file,
                                     SpecificId /*specific_id*/) const
    -> TypeId {
  if (!return_type_inst_id.has_value()) {
    return TypeId::None;
  }
  return file.types().GetTypeIdForTypeInstId(return_type_inst_id);
}

}  // namespace TinySwift::SemIR
