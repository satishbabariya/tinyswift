// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "toolchain/tiny_sil_gen/sil_gen.h"

#include <memory>

#include "llvm/ADT/DenseSet.h"
#include "toolchain/sem_ir/function.h"
#include "toolchain/sem_ir/typed_insts.h"
#include "toolchain/tiny_sil/instruction.h"
#include "toolchain/tiny_sil_gen/context.h"

namespace TinySwift::TinySILGen {

namespace {

// Creates a SIL instruction with a result value.
auto MakeInst(TinySIL::SILInstKind kind, TinySIL::SILValue result)
    -> std::unique_ptr<TinySIL::SILInstruction> {
  auto inst = std::make_unique<TinySIL::SILInstruction>();
  inst->kind = kind;
  inst->result = result;
  return inst;
}

// Creates a SIL instruction with no result.
auto MakeVoidInst(TinySIL::SILInstKind kind)
    -> std::unique_ptr<TinySIL::SILInstruction> {
  auto inst = std::make_unique<TinySIL::SILInstruction>();
  inst->kind = kind;
  inst->result = TinySIL::SILValue::None();
  return inst;
}

// Allocates a new SIL value in the current function.
auto AllocValue(Context& ctx, TinySIL::SILType type) -> TinySIL::SILValue {
  return TinySIL::SILValue{.type = type, .id = ctx.allocateValueId()};
}

// Emits a builtin binary operation (e.g., "add_Int64").
auto EmitBuiltinBinary(Context& ctx, llvm::StringRef builtin_name,
                       SemIR::InstId lhs_id, SemIR::InstId rhs_id,
                       SemIR::TypeId type_id) -> TinySIL::SILValue {
  auto result = AllocValue(ctx, ctx.GetSILType(type_id));
  auto inst = MakeInst(TinySIL::SILInstKind::BuiltinInst, result);
  inst->builtin_name = std::string(builtin_name);
  inst->setOperand(0, ctx.GetValue(lhs_id));
  inst->setOperand(1, ctx.GetValue(rhs_id));
  return ctx.emit(std::move(inst));
}

// Emits a builtin unary operation.
auto EmitBuiltinUnary(Context& ctx, llvm::StringRef builtin_name,
                      SemIR::InstId operand_id, SemIR::TypeId type_id)
    -> TinySIL::SILValue {
  auto result = AllocValue(ctx, ctx.GetSILType(type_id));
  auto inst = MakeInst(TinySIL::SILInstKind::BuiltinInst, result);
  inst->builtin_name = std::string(builtin_name);
  inst->setOperand(0, ctx.GetValue(operand_id));
  return ctx.emit(std::move(inst));
}

// Emits a single SemIR instruction as TinySIL.
auto EmitInst(Context& ctx, SemIR::InstId inst_id) -> void {
  auto& sem_ir = ctx.sem_ir();
  auto inst = sem_ir.insts().Get(inst_id);
  auto kind = inst.kind();

  // Skip compile-time-only instructions.
  if (kind == SemIR::InstKind::Namespace ||
      kind == SemIR::InstKind::ImportDecl ||
      kind == SemIR::InstKind::FunctionDecl ||
      kind == SemIR::InstKind::NameBindingDecl ||
      kind == SemIR::InstKind::ValueBindingPattern ||
      kind == SemIR::InstKind::ValueParamPattern ||
      kind == SemIR::InstKind::SpliceBlock ||
      kind == SemIR::InstKind::BoundMethod) {
    // BoundMethod is a compile-time construct that is always consumed by
    // HandleCallExpr; it never appears as a standalone runtime instruction.
    return;
  }

  // Skip type instructions.
  if (kind == SemIR::InstKind::BoolType ||
      kind == SemIR::InstKind::IntType ||
      kind == SemIR::InstKind::IntLiteralType ||
      kind == SemIR::InstKind::FunctionType ||
      kind == SemIR::InstKind::PointerType ||
      kind == SemIR::InstKind::TypeType ||
      kind == SemIR::InstKind::NamespaceType ||
      kind == SemIR::InstKind::StringType ||
      kind == SemIR::InstKind::FloatType ||
      kind == SemIR::InstKind::DoubleType ||
      kind == SemIR::InstKind::StructType ||
      kind == SemIR::InstKind::ClassType ||
      kind == SemIR::InstKind::EnumDecl ||
      kind == SemIR::InstKind::EnumCase ||
      kind == SemIR::InstKind::EnumCaseWithPayload ||
      kind == SemIR::InstKind::TupleType ||
      kind == SemIR::InstKind::OptionalType ||
      kind == SemIR::InstKind::StructField) {
    return;
  }

  // Skip error instructions.
  if (kind == SemIR::InstKind::ErrorInst) {
    return;
  }

  // --- Literals ---

  if (auto int_val = inst.TryAs<SemIR::IntValue>()) {
    auto result = AllocValue(ctx, ctx.GetSILType(int_val->type_id));
    auto sil_inst =
        MakeInst(TinySIL::SILInstKind::IntegerLiteral, result);
    auto ap_int = sem_ir.ints().Get(int_val->int_id);
    sil_inst->literal_value = ap_int.getSExtValue();
    ctx.emit(std::move(sil_inst));
    ctx.SetValue(inst_id, result);
    return;
  }

  if (auto float_val = inst.TryAs<SemIR::FloatValue>()) {
    auto result = AllocValue(ctx, ctx.GetSILType(float_val->type_id));
    auto sil_inst =
        MakeInst(TinySIL::SILInstKind::FloatLiteral, result);
    auto ap_float = sem_ir.floats().Get(float_val->float_id);
    sil_inst->float_literal_value = ap_float.convertToDouble();
    ctx.emit(std::move(sil_inst));
    ctx.SetValue(inst_id, result);
    return;
  }

  if (auto bool_lit = inst.TryAs<SemIR::BoolLiteral>()) {
    auto result = AllocValue(ctx, ctx.GetSILType(bool_lit->type_id));
    auto sil_inst =
        MakeInst(TinySIL::SILInstKind::IntegerLiteral, result);
    sil_inst->literal_value =
        (bool_lit->value == SemIR::BoolValue::True) ? 1 : 0;
    ctx.emit(std::move(sil_inst));
    ctx.SetValue(inst_id, result);
    return;
  }

  if (auto str_lit = inst.TryAs<SemIR::StringLiteral>()) {
    auto result = AllocValue(ctx, ctx.GetSILType(str_lit->type_id));
    auto sil_inst =
        MakeInst(TinySIL::SILInstKind::StringLiteral, result);
    sil_inst->string_literal_value =
        std::string(sem_ir.string_literal_values().Get(str_lit->string_id));
    ctx.emit(std::move(sil_inst));
    ctx.SetValue(inst_id, result);
    return;
  }

  // --- Variables and Names ---

  if (auto var_storage = inst.TryAs<SemIR::VarStorage>()) {
    auto addr_type = ctx.GetSILType(var_storage->type_id).getAddressType();
    auto result = AllocValue(ctx, addr_type);
    auto sil_inst =
        MakeInst(TinySIL::SILInstKind::AllocStack, result);
    sil_inst->alloc_type = ctx.GetSILType(var_storage->type_id);
    ctx.emit(std::move(sil_inst));
    ctx.SetValue(inst_id, result);
    return;
  }

  if (auto name_ref = inst.TryAs<SemIR::NameRef>()) {
    // A name reference forwards the underlying value.
    // If the referenced value is mutable storage (VarStorage / alloc_stack),
    // emit a load to get the actual stored value.
    auto ref_inst = sem_ir.insts().Get(name_ref->value_id);
    if (ref_inst.Is<SemIR::VarStorage>()) {
      auto addr_value = ctx.GetValue(name_ref->value_id);
      if (addr_value.is_valid()) {
        auto result = AllocValue(ctx, ctx.GetSILType(name_ref->type_id));
        auto sil_inst = MakeInst(TinySIL::SILInstKind::Load, result);
        sil_inst->setOperand(0, addr_value);
        ctx.emit(std::move(sil_inst));
        ctx.SetValue(inst_id, result);
      }
    } else {
      auto value = ctx.GetValue(name_ref->value_id);
      if (value.is_valid()) {
        ctx.SetValue(inst_id, value);
      }
    }
    return;
  }

  if (auto value_binding = inst.TryAs<SemIR::ValueBinding>()) {
    auto value = ctx.GetValue(value_binding->value_id);
    if (value.is_valid()) {
      ctx.SetValue(inst_id, value);
    }
    return;
  }

  if (auto value_param = inst.TryAs<SemIR::ValueParam>()) {
    // Value params are set up during function prologue — should already exist.
    // If not, it's an entry block argument and will be handled there.
    if (!ctx.HasValue(inst_id)) {
      auto result = AllocValue(ctx, ctx.GetSILType(value_param->type_id));
      ctx.SetValue(inst_id, result);
    }
    return;
  }

  if (auto converted = inst.TryAs<SemIR::Converted>()) {
    auto value = ctx.GetValue(converted->result_id);
    if (value.is_valid()) {
      ctx.SetValue(inst_id, value);
    }
    return;
  }

  // --- Assignment ---

  if (auto assign = inst.TryAs<SemIR::Assign>()) {
    auto rhs = ctx.GetValue(assign->rhs_id);

    // For the store destination, we need the raw alloca address (pointer).
    // If lhs_id is a NameRef pointing to VarStorage, look through it to get
    // the alloca address directly rather than the loaded value.
    SemIR::InstId store_addr_id = assign->lhs_id;
    auto lhs_inst = sem_ir.insts().Get(assign->lhs_id);
    if (auto name_ref = lhs_inst.TryAs<SemIR::NameRef>()) {
      auto ref_inst = sem_ir.insts().Get(name_ref->value_id);
      if (ref_inst.Is<SemIR::VarStorage>()) {
        store_addr_id = name_ref->value_id;
      }
    }
    auto lhs = ctx.GetValue(store_addr_id);

    if (rhs.is_valid() && lhs.is_valid()) {
      auto sil_inst = MakeVoidInst(TinySIL::SILInstKind::Store);
      sil_inst->setOperand(0, rhs);
      sil_inst->setOperand(1, lhs);
      ctx.emit(std::move(sil_inst));
    }
    return;
  }

  // --- Integer Arithmetic ---

  if (auto int_add = inst.TryAs<SemIR::IntAdd>()) {
    auto result = EmitBuiltinBinary(ctx, "add_Int64", int_add->lhs_id,
                                    int_add->rhs_id, int_add->type_id);
    ctx.SetValue(inst_id, result);
    return;
  }

  if (auto int_sub = inst.TryAs<SemIR::IntSub>()) {
    auto result = EmitBuiltinBinary(ctx, "sub_Int64", int_sub->lhs_id,
                                    int_sub->rhs_id, int_sub->type_id);
    ctx.SetValue(inst_id, result);
    return;
  }

  if (auto int_mul = inst.TryAs<SemIR::IntMul>()) {
    auto result = EmitBuiltinBinary(ctx, "mul_Int64", int_mul->lhs_id,
                                    int_mul->rhs_id, int_mul->type_id);
    ctx.SetValue(inst_id, result);
    return;
  }

  if (auto int_div = inst.TryAs<SemIR::IntDiv>()) {
    auto result = EmitBuiltinBinary(ctx, "sdiv_Int64", int_div->lhs_id,
                                    int_div->rhs_id, int_div->type_id);
    ctx.SetValue(inst_id, result);
    return;
  }

  if (auto int_mod = inst.TryAs<SemIR::IntMod>()) {
    auto result = EmitBuiltinBinary(ctx, "srem_Int64", int_mod->lhs_id,
                                    int_mod->rhs_id, int_mod->type_id);
    ctx.SetValue(inst_id, result);
    return;
  }

  if (auto int_negate = inst.TryAs<SemIR::IntNegate>()) {
    auto result = EmitBuiltinUnary(ctx, "neg_Int64", int_negate->operand_id,
                                   int_negate->type_id);
    ctx.SetValue(inst_id, result);
    return;
  }

  // --- Integer Comparisons ---

  if (auto int_eq = inst.TryAs<SemIR::IntEq>()) {
    auto result = EmitBuiltinBinary(ctx, "cmp_eq_Int64", int_eq->lhs_id,
                                    int_eq->rhs_id, int_eq->type_id);
    ctx.SetValue(inst_id, result);
    return;
  }

  if (auto int_neq = inst.TryAs<SemIR::IntNeq>()) {
    auto result = EmitBuiltinBinary(ctx, "cmp_ne_Int64", int_neq->lhs_id,
                                    int_neq->rhs_id, int_neq->type_id);
    ctx.SetValue(inst_id, result);
    return;
  }

  if (auto int_less = inst.TryAs<SemIR::IntLess>()) {
    auto result = EmitBuiltinBinary(ctx, "cmp_slt_Int64", int_less->lhs_id,
                                    int_less->rhs_id, int_less->type_id);
    ctx.SetValue(inst_id, result);
    return;
  }

  if (auto int_greater = inst.TryAs<SemIR::IntGreater>()) {
    auto result = EmitBuiltinBinary(
        ctx, "cmp_sgt_Int64", int_greater->lhs_id, int_greater->rhs_id,
        int_greater->type_id);
    ctx.SetValue(inst_id, result);
    return;
  }

  if (auto int_less_eq = inst.TryAs<SemIR::IntLessEq>()) {
    auto result = EmitBuiltinBinary(
        ctx, "cmp_sle_Int64", int_less_eq->lhs_id, int_less_eq->rhs_id,
        int_less_eq->type_id);
    ctx.SetValue(inst_id, result);
    return;
  }

  if (auto int_greater_eq = inst.TryAs<SemIR::IntGreaterEq>()) {
    auto result = EmitBuiltinBinary(
        ctx, "cmp_sge_Int64", int_greater_eq->lhs_id,
        int_greater_eq->rhs_id, int_greater_eq->type_id);
    ctx.SetValue(inst_id, result);
    return;
  }

  // --- Float Arithmetic ---

  if (auto float_add = inst.TryAs<SemIR::FloatAdd>()) {
    auto result = EmitBuiltinBinary(ctx, "fadd_FPIEEE64", float_add->lhs_id,
                                    float_add->rhs_id, float_add->type_id);
    ctx.SetValue(inst_id, result);
    return;
  }

  if (auto float_sub = inst.TryAs<SemIR::FloatSub>()) {
    auto result = EmitBuiltinBinary(ctx, "fsub_FPIEEE64", float_sub->lhs_id,
                                    float_sub->rhs_id, float_sub->type_id);
    ctx.SetValue(inst_id, result);
    return;
  }

  if (auto float_mul = inst.TryAs<SemIR::FloatMul>()) {
    auto result = EmitBuiltinBinary(ctx, "fmul_FPIEEE64", float_mul->lhs_id,
                                    float_mul->rhs_id, float_mul->type_id);
    ctx.SetValue(inst_id, result);
    return;
  }

  if (auto float_div = inst.TryAs<SemIR::FloatDiv>()) {
    auto result = EmitBuiltinBinary(ctx, "fdiv_FPIEEE64", float_div->lhs_id,
                                    float_div->rhs_id, float_div->type_id);
    ctx.SetValue(inst_id, result);
    return;
  }

  if (auto float_negate = inst.TryAs<SemIR::FloatNegate>()) {
    auto result = EmitBuiltinUnary(ctx, "fneg_FPIEEE64",
                                   float_negate->operand_id,
                                   float_negate->type_id);
    ctx.SetValue(inst_id, result);
    return;
  }

  // --- Float Comparisons ---

  if (auto float_eq = inst.TryAs<SemIR::FloatEq>()) {
    auto result = EmitBuiltinBinary(ctx, "fcmp_oeq_FPIEEE64",
                                    float_eq->lhs_id, float_eq->rhs_id,
                                    float_eq->type_id);
    ctx.SetValue(inst_id, result);
    return;
  }

  if (auto float_neq = inst.TryAs<SemIR::FloatNeq>()) {
    auto result = EmitBuiltinBinary(ctx, "fcmp_one_FPIEEE64",
                                    float_neq->lhs_id, float_neq->rhs_id,
                                    float_neq->type_id);
    ctx.SetValue(inst_id, result);
    return;
  }

  if (auto float_less = inst.TryAs<SemIR::FloatLess>()) {
    auto result = EmitBuiltinBinary(
        ctx, "fcmp_olt_FPIEEE64", float_less->lhs_id, float_less->rhs_id,
        float_less->type_id);
    ctx.SetValue(inst_id, result);
    return;
  }

  if (auto float_greater = inst.TryAs<SemIR::FloatGreater>()) {
    auto result = EmitBuiltinBinary(
        ctx, "fcmp_ogt_FPIEEE64", float_greater->lhs_id,
        float_greater->rhs_id, float_greater->type_id);
    ctx.SetValue(inst_id, result);
    return;
  }

  if (auto float_less_eq = inst.TryAs<SemIR::FloatLessEq>()) {
    auto result = EmitBuiltinBinary(
        ctx, "fcmp_ole_FPIEEE64", float_less_eq->lhs_id,
        float_less_eq->rhs_id, float_less_eq->type_id);
    ctx.SetValue(inst_id, result);
    return;
  }

  if (auto float_greater_eq = inst.TryAs<SemIR::FloatGreaterEq>()) {
    auto result = EmitBuiltinBinary(
        ctx, "fcmp_oge_FPIEEE64", float_greater_eq->lhs_id,
        float_greater_eq->rhs_id, float_greater_eq->type_id);
    ctx.SetValue(inst_id, result);
    return;
  }

  // --- Boolean Operations ---

  if (auto bool_not = inst.TryAs<SemIR::BoolNot>()) {
    auto result = EmitBuiltinUnary(ctx, "xor_Int1", bool_not->operand_id,
                                   bool_not->type_id);
    ctx.SetValue(inst_id, result);
    return;
  }

  if (auto bool_and = inst.TryAs<SemIR::BoolAnd>()) {
    auto result = EmitBuiltinBinary(ctx, "and_Int1", bool_and->lhs_id,
                                    bool_and->rhs_id, bool_and->type_id);
    ctx.SetValue(inst_id, result);
    return;
  }

  if (auto bool_or = inst.TryAs<SemIR::BoolOr>()) {
    auto result = EmitBuiltinBinary(ctx, "or_Int1", bool_or->lhs_id,
                                    bool_or->rhs_id, bool_or->type_id);
    ctx.SetValue(inst_id, result);
    return;
  }

  // --- Conversions ---

  if (auto int_to_float = inst.TryAs<SemIR::IntToFloat>()) {
    auto result = EmitBuiltinUnary(ctx, "sitofp_Int64_FPIEEE64",
                                   int_to_float->operand_id,
                                   int_to_float->type_id);
    ctx.SetValue(inst_id, result);
    return;
  }

  if (auto float_to_int = inst.TryAs<SemIR::FloatToInt>()) {
    auto result = EmitBuiltinUnary(ctx, "fptosi_FPIEEE64_Int64",
                                   float_to_int->operand_id,
                                   float_to_int->type_id);
    ctx.SetValue(inst_id, result);
    return;
  }

  // --- String Operations ---

  if (auto string_concat = inst.TryAs<SemIR::StringConcat>()) {
    auto result = EmitBuiltinBinary(ctx, "string_concat",
                                    string_concat->lhs_id,
                                    string_concat->rhs_id,
                                    string_concat->type_id);
    ctx.SetValue(inst_id, result);
    return;
  }

  // --- Function Calls ---

  if (auto call = inst.TryAs<SemIR::Call>()) {
    // Get the callee, looking through NameRef wrappers.
    auto callee_inst = sem_ir.insts().Get(call->callee_id);
    if (auto name_ref = callee_inst.TryAs<SemIR::NameRef>()) {
      callee_inst = sem_ir.insts().Get(name_ref->value_id);
    }

    // Resolve function name from FunctionDecl, FunctionType, ClosureExpr,
    // or a ValueBinding that holds a closure.
    std::string func_name;
    if (auto fn_decl = callee_inst.TryAs<SemIR::FunctionDecl>()) {
      auto& function = sem_ir.functions().Get(fn_decl->function_id);
      func_name = ctx.GetFunctionName(function);
    } else if (auto func_type = callee_inst.TryAs<SemIR::FunctionType>()) {
      auto& function = sem_ir.functions().Get(func_type->function_id);
      func_name = ctx.GetFunctionName(function);
    } else if (auto closure_expr = callee_inst.TryAs<SemIR::ClosureExpr>()) {
      // Direct closure call: `{ x in x+x }(5)`.
      auto& function = sem_ir.functions().Get(closure_expr->function_id);
      func_name = ctx.GetFunctionName(function);
    } else if (auto value_binding = callee_inst.TryAs<SemIR::ValueBinding>()) {
      // Let binding holding a closure: `let f = { x in x+x }; f(5)`.
      if (value_binding->value_id.has_value()) {
        auto inner_inst = sem_ir.insts().Get(value_binding->value_id);
        if (auto closure_expr = inner_inst.TryAs<SemIR::ClosureExpr>()) {
          auto& function = sem_ir.functions().Get(closure_expr->function_id);
          func_name = ctx.GetFunctionName(function);
        }
      }
      if (func_name.empty()) func_name = "<unknown_callee>";
    } else {
      func_name = "<unknown_callee>";
    }

    // Emit function_ref.
    auto fn_ref_type = ctx.GetSILType(callee_inst.type_id());
    auto fn_ref_val = AllocValue(ctx, fn_ref_type);
    auto fn_ref_inst =
        MakeInst(TinySIL::SILInstKind::FunctionRef, fn_ref_val);
    fn_ref_inst->function_name = func_name;
    ctx.emit(std::move(fn_ref_inst));

    // Emit apply.
    auto result_type = ctx.GetSILType(call->type_id);
    auto result = AllocValue(ctx, result_type);
    auto apply_inst =
        MakeInst(TinySIL::SILInstKind::Apply, result);
    apply_inst->setOperand(0, fn_ref_val);

    // Arguments.
    if (call->args_id.has_value() &&
        call->args_id != SemIR::InstBlockId::Empty) {
      auto arg_ids = sem_ir.inst_blocks().Get(call->args_id);
      for (auto arg_id : arg_ids) {
        auto arg_val = ctx.GetValue(arg_id);
        if (arg_val.is_valid()) {
          apply_inst->operand_list.push_back(arg_val);
        }
      }
    }

    ctx.emit(std::move(apply_inst));
    ctx.SetValue(inst_id, result);
    return;
  }

  // --- Control Flow ---

  if (auto branch = inst.TryAs<SemIR::Branch>()) {
    auto sil_inst = MakeVoidInst(TinySIL::SILInstKind::Branch);
    sil_inst->target_block = ctx.GetBlock(branch->target_id);
    ctx.emit(std::move(sil_inst));
    return;
  }

  if (auto branch_if = inst.TryAs<SemIR::BranchIf>()) {
    auto sil_inst = MakeVoidInst(TinySIL::SILInstKind::CondBranch);
    sil_inst->setOperand(0, ctx.GetValue(branch_if->cond_id));
    sil_inst->true_block = ctx.GetBlock(branch_if->target_id);
    // The false block is the next block (fallthrough). We look for the
    // next Branch instruction in the same sequence.
    // For now, mark false_block as -1; we'll fix it in a post-pass or
    // the next instruction should be an unconditional Branch.
    sil_inst->false_block = -1;
    ctx.emit(std::move(sil_inst));
    return;
  }

  if (auto branch_with_arg = inst.TryAs<SemIR::BranchWithArg>()) {
    auto sil_inst = MakeVoidInst(TinySIL::SILInstKind::Branch);
    sil_inst->target_block = ctx.GetBlock(branch_with_arg->target_id);
    auto arg_val = ctx.GetValue(branch_with_arg->arg_id);
    if (arg_val.is_valid()) {
      sil_inst->branch_args.push_back(arg_val);
    }
    ctx.emit(std::move(sil_inst));
    return;
  }

  if (auto block_arg = inst.TryAs<SemIR::BlockArg>()) {
    // Block arguments are set up during block creation.
    if (!ctx.HasValue(inst_id)) {
      auto result = AllocValue(ctx, ctx.GetSILType(block_arg->type_id));
      ctx.SetValue(inst_id, result);
    }
    return;
  }

  // --- Return ---

  if (inst.Is<SemIR::Return>()) {
    // Void return: in SIL, return a tuple () placeholder.
    auto result_type = TinySIL::SILType{};
    auto result = AllocValue(ctx, result_type);
    // Emit a tuple() for void return.
    auto tuple_inst = MakeInst(TinySIL::SILInstKind::TupleInst, result);
    ctx.emit(std::move(tuple_inst));
    auto sil_inst = MakeVoidInst(TinySIL::SILInstKind::ReturnInst);
    sil_inst->setOperand(0, result);
    ctx.emit(std::move(sil_inst));
    return;
  }

  if (auto return_expr = inst.TryAs<SemIR::ReturnExpr>()) {
    auto sil_inst = MakeVoidInst(TinySIL::SILInstKind::ReturnInst);
    sil_inst->setOperand(0, ctx.GetValue(return_expr->expr_id));
    ctx.emit(std::move(sil_inst));
    return;
  }

  // --- Struct Operations ---

  if (auto struct_init = inst.TryAs<SemIR::StructInit>()) {
    auto result = AllocValue(ctx, ctx.GetSILType(struct_init->type_id));
    auto sil_inst = MakeInst(TinySIL::SILInstKind::StructInst, result);
    if (struct_init->args_id.has_value() &&
        struct_init->args_id != SemIR::InstBlockId::Empty) {
      auto arg_ids = sem_ir.inst_blocks().Get(struct_init->args_id);
      for (auto arg_id : arg_ids) {
        auto arg_val = ctx.GetValue(arg_id);
        if (arg_val.is_valid()) {
          sil_inst->operand_list.push_back(arg_val);
        }
      }
    }
    ctx.emit(std::move(sil_inst));
    ctx.SetValue(inst_id, result);
    return;
  }

  if (auto field_access = inst.TryAs<SemIR::FieldAccess>()) {
    auto result = AllocValue(ctx, ctx.GetSILType(field_access->type_id));
    auto sil_inst =
        MakeInst(TinySIL::SILInstKind::StructExtract, result);
    sil_inst->setOperand(0, ctx.GetValue(field_access->base_id));
    sil_inst->element_index = field_access->index.index;
    ctx.emit(std::move(sil_inst));
    ctx.SetValue(inst_id, result);
    return;
  }

  // --- Enum Operations ---

  if (auto enum_init = inst.TryAs<SemIR::EnumInit>()) {
    auto result = AllocValue(ctx, ctx.GetSILType(enum_init->type_id));
    auto sil_inst = MakeInst(TinySIL::SILInstKind::EnumInst, result);
    // Extract the discriminant value from the referenced EnumCase instruction.
    auto case_inst = sem_ir.insts().Get(enum_init->case_id);
    if (auto ec = case_inst.TryAs<SemIR::EnumCase>()) {
      sil_inst->literal_value = ec->discriminant.index;
    }
    ctx.emit(std::move(sil_inst));
    ctx.SetValue(inst_id, result);
    return;
  }

  if (auto enum_disc = inst.TryAs<SemIR::EnumDiscriminant>()) {
    auto result = AllocValue(ctx, ctx.GetSILType(enum_disc->type_id));
    // Use a builtin to extract the discriminant.
    auto sil_inst = MakeInst(TinySIL::SILInstKind::BuiltinInst, result);
    sil_inst->builtin_name = "enum_discriminant";
    sil_inst->setOperand(0, ctx.GetValue(enum_disc->enum_id));
    ctx.emit(std::move(sil_inst));
    ctx.SetValue(inst_id, result);
    return;
  }

  if (auto enum_payload = inst.TryAs<SemIR::EnumPayload>()) {
    auto result = AllocValue(ctx, ctx.GetSILType(enum_payload->type_id));
    auto sil_inst =
        MakeInst(TinySIL::SILInstKind::UncheckedEnumData, result);
    sil_inst->setOperand(0, ctx.GetValue(enum_payload->enum_id));
    ctx.emit(std::move(sil_inst));
    ctx.SetValue(inst_id, result);
    return;
  }

  // --- Tuple Operations ---

  if (auto tuple_init = inst.TryAs<SemIR::TupleInit>()) {
    auto result = AllocValue(ctx, ctx.GetSILType(tuple_init->type_id));
    auto sil_inst = MakeInst(TinySIL::SILInstKind::TupleInst, result);
    if (tuple_init->elements_id.has_value() &&
        tuple_init->elements_id != SemIR::InstBlockId::Empty) {
      auto elem_ids = sem_ir.inst_blocks().Get(tuple_init->elements_id);
      for (auto elem_id : elem_ids) {
        auto elem_val = ctx.GetValue(elem_id);
        if (elem_val.is_valid()) {
          sil_inst->operand_list.push_back(elem_val);
        }
      }
    }
    ctx.emit(std::move(sil_inst));
    ctx.SetValue(inst_id, result);
    return;
  }

  if (auto tuple_access = inst.TryAs<SemIR::TupleAccess>()) {
    auto result = AllocValue(ctx, ctx.GetSILType(tuple_access->type_id));
    auto sil_inst =
        MakeInst(TinySIL::SILInstKind::TupleExtract, result);
    sil_inst->setOperand(0, ctx.GetValue(tuple_access->tuple_id));
    sil_inst->element_index = tuple_access->index.index;
    ctx.emit(std::move(sil_inst));
    ctx.SetValue(inst_id, result);
    return;
  }

  // --- Optional Operations ---

  if (auto opt_none = inst.TryAs<SemIR::OptionalNone>()) {
    // Optional<T> is represented as {i1, T} — None = {false, T(zero)}.
    auto bool_type_id = SemIR::TypeId::ForTypeConstant(
        SemIR::ConstantId::ForConcreteConstant(SemIR::BoolType::TypeInstId));

    // Determine the inner type from Optional<T>.
    SemIR::TypeId inner_type_id = opt_none->type_id;
    auto opt_ti = sem_ir.types().GetTypeInstId(opt_none->type_id);
    if (opt_ti.has_value()) {
      auto opt_type_inst = sem_ir.insts().Get(opt_ti);
      if (auto ot = opt_type_inst.TryAs<SemIR::OptionalType>()) {
        inner_type_id =
            sem_ir.types().GetTypeIdForTypeInstId(ot->inner_type_id);
      }
    }

    // Emit has-value flag = i1 false.
    auto flag_val = AllocValue(ctx, ctx.GetSILType(bool_type_id));
    auto flag_inst = MakeInst(TinySIL::SILInstKind::IntegerLiteral, flag_val);
    flag_inst->literal_value = 0;
    ctx.emit(std::move(flag_inst));

    // Emit zero payload for inner type.
    auto payload_val = AllocValue(ctx, ctx.GetSILType(inner_type_id));
    auto payload_inst =
        MakeInst(TinySIL::SILInstKind::IntegerLiteral, payload_val);
    payload_inst->literal_value = 0;
    ctx.emit(std::move(payload_inst));

    // Emit TupleInst {i1 false, T 0} — matches {i1, T} Optional layout.
    auto result = AllocValue(ctx, ctx.GetSILType(opt_none->type_id));
    auto tuple_inst = MakeInst(TinySIL::SILInstKind::TupleInst, result);
    tuple_inst->operand_list.push_back(flag_val);
    tuple_inst->operand_list.push_back(payload_val);
    ctx.emit(std::move(tuple_inst));
    ctx.SetValue(inst_id, result);
    return;
  }

  if (auto opt_some = inst.TryAs<SemIR::OptionalSome>()) {
    // Optional<T> is represented as {i1, T} — Some(x) = {true, x}.
    auto bool_type_id = SemIR::TypeId::ForTypeConstant(
        SemIR::ConstantId::ForConcreteConstant(SemIR::BoolType::TypeInstId));

    // Emit has-value flag = i1 true.
    auto flag_val = AllocValue(ctx, ctx.GetSILType(bool_type_id));
    auto flag_inst = MakeInst(TinySIL::SILInstKind::IntegerLiteral, flag_val);
    flag_inst->literal_value = 1;
    ctx.emit(std::move(flag_inst));

    // Emit TupleInst {i1 true, inner_value}.
    auto inner_val = ctx.GetValue(opt_some->value_id);
    auto result = AllocValue(ctx, ctx.GetSILType(opt_some->type_id));
    auto tuple_inst = MakeInst(TinySIL::SILInstKind::TupleInst, result);
    tuple_inst->operand_list.push_back(flag_val);
    if (inner_val.is_valid()) {
      tuple_inst->operand_list.push_back(inner_val);
    }
    ctx.emit(std::move(tuple_inst));
    ctx.SetValue(inst_id, result);
    return;
  }

  // --- Closure ---

  if (auto closure = inst.TryAs<SemIR::ClosureExpr>()) {
    auto& function = sem_ir.functions().Get(closure->function_id);
    std::string func_name = ctx.GetFunctionName(function);

    // function_ref for the closure body.
    auto fn_ref_type = ctx.GetSILType(closure->type_id);
    auto fn_ref_val = AllocValue(ctx, fn_ref_type);
    auto fn_ref_inst =
        MakeInst(TinySIL::SILInstKind::FunctionRef, fn_ref_val);
    fn_ref_inst->function_name = func_name;
    ctx.emit(std::move(fn_ref_inst));

    // partial_apply with captures.
    auto result = AllocValue(ctx, ctx.GetSILType(closure->type_id));
    auto pa_inst =
        MakeInst(TinySIL::SILInstKind::PartialApply, result);
    pa_inst->setOperand(0, fn_ref_val);
    if (closure->captures_id.has_value() &&
        closure->captures_id != SemIR::InstBlockId::Empty) {
      auto capture_ids = sem_ir.inst_blocks().Get(closure->captures_id);
      for (auto cap_id : capture_ids) {
        auto cap_val = ctx.GetValue(cap_id);
        if (cap_val.is_valid()) {
          pa_inst->operand_list.push_back(cap_val);
        }
      }
    }
    ctx.emit(std::move(pa_inst));
    ctx.SetValue(inst_id, result);
    return;
  }

  if (auto capture_ref = inst.TryAs<SemIR::CaptureRef>()) {
    auto value = ctx.GetValue(capture_ref->captured_inst_id);
    if (value.is_valid()) {
      ctx.SetValue(inst_id, value);
    }
    return;
  }

  // --- Switch ---

  if (auto switch_inst = inst.TryAs<SemIR::SwitchInst>()) {
    auto sil_inst = MakeVoidInst(TinySIL::SILInstKind::SwitchEnum);
    sil_inst->setOperand(0, ctx.GetValue(switch_inst->scrutinee_id));
    ctx.emit(std::move(sil_inst));
    return;
  }

  // --- Debug ---

  // CasePattern: skip for now, handled within SwitchInst lowering.
  if (inst.Is<SemIR::CasePattern>()) {
    return;
  }

  // Fall through for unknown instructions — emit debug_value as marker.
  // In a production compiler we'd assert here, but this allows us to
  // make progress on the rest of the pipeline.
}

// Emits TinySIL for a function body.
auto EmitFunctionBody(Context& ctx, SemIR::FunctionId func_id,
                      const SemIR::Function& function) -> void {
  auto& sem_ir = ctx.sem_ir();
  std::string func_name = ctx.GetFunctionName(function);

  auto* sil_fn = ctx.sil_module().createFunction(func_name);
  sil_fn->sem_ir_function_id = func_id.index;
  ctx.set_current_function(sil_fn);

  if (function.body_block_ids.empty()) {
    sil_fn->is_declaration = true;
    ctx.clearFunctionState();
    return;
  }

  // Build the SIL function type.
  if (function.return_type_inst_id.has_value()) {
    auto ret_type_id =
        sem_ir.types().GetTypeIdForTypeInstId(function.return_type_inst_id);
    sil_fn->type.return_type = ctx.GetSILType(ret_type_id);
    sil_fn->type.is_void_return = false;
  } else {
    sil_fn->type.is_void_return = true;
  }

  // Collect parameter types.
  if (function.call_params_id.has_value() &&
      function.call_params_id != SemIR::InstBlockId::Empty) {
    auto param_ids = sem_ir.inst_blocks().Get(function.call_params_id);
    for (auto param_id : param_ids) {
      auto param_inst = sem_ir.insts().Get(param_id);
      if (auto value_param = param_inst.TryAs<SemIR::ValueParam>()) {
        auto param_type = ctx.GetSILType(value_param->type_id);
        sil_fn->type.param_types.push_back(param_type);
        sil_fn->type.param_conventions.push_back(TinySIL::Ownership::Owned);
      }
    }
  }

  // Create basic blocks for all body blocks upfront.
  for (size_t i = 0; i < function.body_block_ids.size(); ++i) {
    auto* bb = sil_fn->createBasicBlock();
    ctx.SetBlock(function.body_block_ids[i], bb->id);
  }

  // Map function parameters to entry block arguments.
  if (function.call_params_id.has_value() &&
      function.call_params_id != SemIR::InstBlockId::Empty) {
    auto* entry_bb = sil_fn->getEntryBlock();
    auto param_ids = sem_ir.inst_blocks().Get(function.call_params_id);
    for (auto param_id : param_ids) {
      auto param_inst = sem_ir.insts().Get(param_id);
      if (auto value_param = param_inst.TryAs<SemIR::ValueParam>()) {
        auto param_type = ctx.GetSILType(value_param->type_id);
        auto arg_val = AllocValue(ctx, param_type);
        entry_bb->args.push_back(arg_val);
        ctx.SetValue(param_id, arg_val);
      }
    }
  }

  // Emit instructions for each block.
  for (size_t block_idx = 0; block_idx < function.body_block_ids.size();
       ++block_idx) {
    auto block_id = function.body_block_ids[block_idx];
    auto sil_block_id = ctx.GetBlock(block_id);
    auto* bb = sil_fn->blocks[sil_block_id].get();
    ctx.set_current_block(bb);

    auto block_insts = sem_ir.inst_blocks().Get(block_id);
    for (auto inst_id : block_insts) {
      EmitInst(ctx, inst_id);
    }

    // Ensure the block has a terminator.
    if (!bb->is_terminated() && !bb->insts.empty()) {
      // Add an unreachable as fallback.
      auto unreachable = MakeVoidInst(TinySIL::SILInstKind::Unreachable);
      bb->addInst(std::move(unreachable));
    } else if (bb->insts.empty()) {
      // Empty block: add unreachable.
      auto unreachable = MakeVoidInst(TinySIL::SILInstKind::Unreachable);
      bb->addInst(std::move(unreachable));
    }
  }

  // Fix up cond_br false blocks. In SemIR, BranchIf is followed by Branch
  // for the false case. In SIL, cond_br takes both targets directly.
  for (auto& bb : sil_fn->blocks) {
    for (size_t i = 0; i < bb->insts.size(); ++i) {
      auto* sil_inst = bb->insts[i].get();
      if (sil_inst->kind == TinySIL::SILInstKind::CondBranch &&
          sil_inst->false_block == -1) {
        // Look for the next instruction — it should be a Branch.
        if (i + 1 < bb->insts.size() &&
            bb->insts[i + 1]->kind == TinySIL::SILInstKind::Branch) {
          sil_inst->false_block = bb->insts[i + 1]->target_block;
          // Remove the redundant Branch.
          bb->insts.erase(bb->insts.begin() + i + 1);
        }
      }
    }
  }

  ctx.clearFunctionState();
}

}  // namespace

auto GenerateSIL(const SemIR::File& sem_ir)
    -> std::unique_ptr<TinySIL::SILModule> {
  auto sil_module = std::make_unique<TinySIL::SILModule>();
  sil_module->name = std::string(sem_ir.filename());

  Context ctx(sem_ir, *sil_module);

  // Build a set of function names that have bodies. When the two-pass check
  // phase registers forward declarations in pass 1 and full definitions in
  // pass 2, we end up with duplicate SemIR Functions (same name). We prefer
  // the one with a body and skip pure declarations for the same name.
  llvm::DenseSet<int32_t> names_with_bodies;
  for (auto [func_id, function] : sem_ir.functions().enumerate()) {
    if (!function.body_block_ids.empty()) {
      auto id = function.name_id.AsIdentifierId();
      if (id.has_value()) {
        names_with_bodies.insert(id.index);
      }
    }
  }

  // Emit all functions, skipping declarations superseded by a body definition.
  for (auto [func_id, function] : sem_ir.functions().enumerate()) {
    auto identifier_id = function.name_id.AsIdentifierId();
    if (!identifier_id.has_value()) {
      continue;
    }
    // Skip declaration-only stubs if a body-having function with the same
    // name exists (it will be emitted later in the loop).
    if (function.body_block_ids.empty() &&
        names_with_bodies.count(identifier_id.index)) {
      continue;
    }
    EmitFunctionBody(ctx, func_id, function);
  }

  // Emit top-level instructions as __tinyswift_init if needed.
  auto top_block_id = sem_ir.top_inst_block_id();
  if (top_block_id.has_value()) {
    auto top_insts = sem_ir.inst_blocks().Get(top_block_id);
    bool has_runtime = false;
    for (auto inst_id : top_insts) {
      auto inst = sem_ir.insts().Get(inst_id);
      auto kind = inst.kind();
      if (kind != SemIR::InstKind::Namespace &&
          kind != SemIR::InstKind::ImportDecl &&
          kind != SemIR::InstKind::FunctionDecl &&
          kind != SemIR::InstKind::NameBindingDecl &&
          kind != SemIR::InstKind::BoolType &&
          kind != SemIR::InstKind::IntType &&
          kind != SemIR::InstKind::IntLiteralType &&
          kind != SemIR::InstKind::FunctionType &&
          kind != SemIR::InstKind::PointerType &&
          kind != SemIR::InstKind::TypeType &&
          kind != SemIR::InstKind::NamespaceType &&
          kind != SemIR::InstKind::StringType &&
          kind != SemIR::InstKind::FloatType &&
          kind != SemIR::InstKind::DoubleType &&
          kind != SemIR::InstKind::StructType &&
          kind != SemIR::InstKind::ClassType &&
          kind != SemIR::InstKind::EnumDecl &&
          kind != SemIR::InstKind::EnumCase &&
          kind != SemIR::InstKind::EnumCaseWithPayload &&
          kind != SemIR::InstKind::TupleType &&
          kind != SemIR::InstKind::OptionalType &&
          kind != SemIR::InstKind::StructField &&
          kind != SemIR::InstKind::BoundMethod &&
          kind != SemIR::InstKind::ValueBindingPattern &&
          kind != SemIR::InstKind::ValueParamPattern) {
        has_runtime = true;
        break;
      }
    }

    if (has_runtime) {
      auto* init_fn = sil_module->createFunction("__tinyswift_init");
      init_fn->type.is_void_return = true;
      ctx.set_current_function(init_fn);

      auto* entry_bb = init_fn->createBasicBlock();
      ctx.set_current_block(entry_bb);

      for (auto inst_id : top_insts) {
        EmitInst(ctx, inst_id);
      }

      // Terminate with return.
      if (!entry_bb->is_terminated()) {
        auto result_type = TinySIL::SILType{};
        auto result = AllocValue(ctx, result_type);
        auto tuple_inst =
            MakeInst(TinySIL::SILInstKind::TupleInst, result);
        ctx.emit(std::move(tuple_inst));
        auto ret_inst = MakeVoidInst(TinySIL::SILInstKind::ReturnInst);
        ret_inst->setOperand(0, result);
        ctx.emit(std::move(ret_inst));
      }

      ctx.clearFunctionState();
    }
  }

  return sil_module;
}

}  // namespace TinySwift::TinySILGen
