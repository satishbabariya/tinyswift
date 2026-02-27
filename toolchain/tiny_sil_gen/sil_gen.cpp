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
      kind == SemIR::InstKind::BoundMethod ||
      kind == SemIR::InstKind::ComputedPropertyDecl) {
    // BoundMethod and ComputedPropertyDecl are compile-time constructs that
    // are always consumed by HandleCallExpr/HandleMemberAccessExpr; they
    // never appear as standalone runtime instructions.
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
      // For array-backed VarStorage (ArrayLiteralInit assigned in check phase),
      // skip the Load — subscript operations use the ALI result directly.
      if (ctx.IsArrayVar(name_ref->value_id.index)) {
        return;
      }
      auto addr_value = ctx.GetValue(name_ref->value_id);
      if (addr_value.is_valid()) {
        auto result = AllocValue(ctx, ctx.GetSILType(name_ref->type_id));
        auto sil_inst = MakeInst(TinySIL::SILInstKind::Load, result);
        sil_inst->setOperand(0, addr_value);
        ctx.emit(std::move(sil_inst));
        ctx.SetValue(inst_id, result);
      }
    } else if (ref_inst.Is<SemIR::InoutParam>()) {
      // M40: InoutParam is a pointer; dereference it to read the value.
      auto ptr_val = ctx.GetValue(name_ref->value_id);
      if (ptr_val.is_valid()) {
        auto result = AllocValue(ctx, ctx.GetSILType(name_ref->type_id));
        auto sil_inst = MakeInst(TinySIL::SILInstKind::Load, result);
        sil_inst->setOperand(0, ptr_val);
        ctx.emit(std::move(sil_inst));
        ctx.SetValue(inst_id, result);
      }
    } else if (ref_inst.Is<SemIR::FunctionDecl>()) {
      // A named function used as a first-class value — emit function_ref
      // to get a callable pointer (e.g., `applyFn(double, 21)`).
      auto fn_decl = ref_inst.As<SemIR::FunctionDecl>();
      auto& function = sem_ir.functions().Get(fn_decl.function_id);
      std::string func_name = ctx.GetFunctionName(function);
      // function_ref has ptr type; use PointerType SIL type.
      auto fn_ref_type = ctx.GetSILType(name_ref->type_id);
      auto fn_ref_val = AllocValue(ctx, fn_ref_type);
      auto fn_ref_inst =
          MakeInst(TinySIL::SILInstKind::FunctionRef, fn_ref_val);
      fn_ref_inst->function_name = func_name;
      ctx.emit(std::move(fn_ref_inst));
      ctx.SetValue(inst_id, fn_ref_val);
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
    auto rhs_inst_check = sem_ir.insts().Get(assign->rhs_id);

    // For the store destination, we need the raw alloca address (pointer).
    // Handle several LHS cases:
    // 1. NameRef → VarStorage: look through to get alloca address.
    // 2. ArrayElementAddr: its sil_gen value is already a GEP pointer.
    // 3. FieldAddr: its sil_gen value is a StructElementAddr (GEP pointer).
    SemIR::InstId store_addr_id = assign->lhs_id;
    auto lhs_inst = sem_ir.insts().Get(assign->lhs_id);
    if (auto name_ref = lhs_inst.TryAs<SemIR::NameRef>()) {
      auto ref_inst = sem_ir.insts().Get(name_ref->value_id);
      if (ref_inst.Is<SemIR::VarStorage>()) {
        store_addr_id = name_ref->value_id;
      } else if (ref_inst.Is<SemIR::InoutParam>()) {
        // M40: InoutParam is a pointer; store through it.
        store_addr_id = name_ref->value_id;
      }
    }

    // Special case: assigning ArrayLiteralInit directly to VarStorage.
    // The check phase resolves array_id to the ALI InstId directly for
    // subscript operations — no Store is needed. Mark the VarStorage so
    // the NameRef handler skips emitting a dead Load (which would confuse
    // the definite-init checker).
    auto lhs_check = sem_ir.insts().Get(store_addr_id);
    if (rhs_inst_check.Is<SemIR::ArrayLiteralInit>() &&
        lhs_check.Is<SemIR::VarStorage>()) {
      ctx.MarkArrayVar(store_addr_id.index);
      return;
    }

    auto rhs = ctx.GetValue(assign->rhs_id);
    // ArrayElementAddr and FieldAddr: their ctx value IS the address — use directly.
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

  // --- Integer Bitwise Operations ---

  if (auto x = inst.TryAs<SemIR::IntBitwiseAnd>()) {
    auto result = EmitBuiltinBinary(ctx, "and_Int64", x->lhs_id, x->rhs_id, x->type_id);
    ctx.SetValue(inst_id, result);
    return;
  }
  if (auto x = inst.TryAs<SemIR::IntBitwiseOr>()) {
    auto result = EmitBuiltinBinary(ctx, "or_Int64", x->lhs_id, x->rhs_id, x->type_id);
    ctx.SetValue(inst_id, result);
    return;
  }
  if (auto x = inst.TryAs<SemIR::IntBitwiseXor>()) {
    auto result = EmitBuiltinBinary(ctx, "xor_Int64", x->lhs_id, x->rhs_id, x->type_id);
    ctx.SetValue(inst_id, result);
    return;
  }
  if (auto x = inst.TryAs<SemIR::IntBitwiseNot>()) {
    auto result = EmitBuiltinUnary(ctx, "not_Int64", x->operand_id, x->type_id);
    ctx.SetValue(inst_id, result);
    return;
  }
  if (auto x = inst.TryAs<SemIR::IntShiftLeft>()) {
    auto result = EmitBuiltinBinary(ctx, "shl_Int64", x->lhs_id, x->rhs_id, x->type_id);
    ctx.SetValue(inst_id, result);
    return;
  }
  if (auto x = inst.TryAs<SemIR::IntShiftRight>()) {
    auto result = EmitBuiltinBinary(ctx, "ashr_Int64", x->lhs_id, x->rhs_id, x->type_id);
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
    auto callee_inst_id = call->callee_id;
    auto callee_inst = sem_ir.insts().Get(callee_inst_id);
    SemIR::InstId resolved_callee_id = callee_inst_id;
    if (auto name_ref = callee_inst.TryAs<SemIR::NameRef>()) {
      resolved_callee_id = name_ref->value_id;
      callee_inst = sem_ir.insts().Get(resolved_callee_id);
    }

    // M46/indirect call: if callee is a ValueParam with function type (higher-
    // order function parameter), use its runtime SIL value directly — no static
    // function_ref.
    if (callee_inst.Is<SemIR::ValueParam>()) {
      auto callee_val = ctx.GetValue(resolved_callee_id);

      auto result_type = ctx.GetSILType(call->type_id);
      auto result = AllocValue(ctx, result_type);
      auto apply_inst = MakeInst(TinySIL::SILInstKind::Apply, result);
      if (callee_val.is_valid()) {
        apply_inst->setOperand(0, callee_val);
      }

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
      // M53: Use the PartialApply result (ctx.GetValue of the ClosureExpr) as
      // callee so lower.cpp can prepend captured values via closure_captures map.
      auto pa_val = ctx.GetValue(resolved_callee_id);
      auto result_type = ctx.GetSILType(call->type_id);
      auto result = AllocValue(ctx, result_type);
      auto apply_inst = MakeInst(TinySIL::SILInstKind::Apply, result);
      if (pa_val.is_valid()) {
        apply_inst->setOperand(0, pa_val);
      }
      if (call->args_id.has_value() &&
          call->args_id != SemIR::InstBlockId::Empty) {
        auto arg_ids = sem_ir.inst_blocks().Get(call->args_id);
        for (auto arg_id : arg_ids) {
          auto arg_val = ctx.GetValue(arg_id);
          if (arg_val.is_valid()) apply_inst->operand_list.push_back(arg_val);
        }
      }
      ctx.emit(std::move(apply_inst));
      ctx.SetValue(inst_id, result);
      return;
    } else if (auto value_binding = callee_inst.TryAs<SemIR::ValueBinding>()) {
      // Let binding holding a closure: `let f = { x in x+x }; f(5)`.
      // M53: Use the PartialApply result stored for the ClosureExpr as callee.
      TinySIL::SILValue closure_pa_val;
      if (value_binding->value_id.has_value()) {
        auto inner_inst = sem_ir.insts().Get(value_binding->value_id);
        if (inner_inst.Is<SemIR::ClosureExpr>()) {
          closure_pa_val = ctx.GetValue(value_binding->value_id);
        }
      }
      if (closure_pa_val.is_valid()) {
        auto result_type = ctx.GetSILType(call->type_id);
        auto result = AllocValue(ctx, result_type);
        auto apply_inst = MakeInst(TinySIL::SILInstKind::Apply, result);
        apply_inst->setOperand(0, closure_pa_val);
        if (call->args_id.has_value() &&
            call->args_id != SemIR::InstBlockId::Empty) {
          auto arg_ids = sem_ir.inst_blocks().Get(call->args_id);
          for (auto arg_id : arg_ids) {
            auto arg_val = ctx.GetValue(arg_id);
            if (arg_val.is_valid()) apply_inst->operand_list.push_back(arg_val);
          }
        }
        ctx.emit(std::move(apply_inst));
        ctx.SetValue(inst_id, result);
        return;
      }
      func_name = "<unknown_callee>";
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

  if (auto field_addr = inst.TryAs<SemIR::FieldAddr>()) {
    // Compute the address of a struct field (for lvalue member assignment).
    // Result is an address type (pointer to the field's storage).
    auto result =
        AllocValue(ctx, ctx.GetSILType(field_addr->type_id).getAddressType());
    auto sil_inst = MakeInst(TinySIL::SILInstKind::StructElementAddr, result);
    sil_inst->setOperand(0, ctx.GetValue(field_addr->base_id));
    sil_inst->element_index = field_addr->index.index;
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

  // --- Array Operations ---

  if (auto ali = inst.TryAs<SemIR::ArrayLiteralInit>()) {
    // Allocate [N x T] on stack, store elements, produce pointer to array.
    // Result uses address type (pointer convention, like AllocStack).
    auto elem_sil_type = ctx.GetSILType(ali->type_id);
    auto result = AllocValue(ctx, elem_sil_type.getAddressType());
    auto sil_inst = MakeInst(TinySIL::SILInstKind::BuiltinInst, result);
    sil_inst->builtin_name = "array_literal_init";
    sil_inst->alloc_type = elem_sil_type;  // element SIL type
    if (ali->elements_id.has_value() &&
        ali->elements_id != SemIR::InstBlockId::Empty) {
      auto elem_ids = sem_ir.inst_blocks().Get(ali->elements_id);
      sil_inst->literal_value = static_cast<int64_t>(elem_ids.size());
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

  if (auto aa = inst.TryAs<SemIR::ArrayAccess>()) {
    auto result = AllocValue(ctx, ctx.GetSILType(aa->type_id));
    auto sil_inst = MakeInst(TinySIL::SILInstKind::BuiltinInst, result);
    sil_inst->builtin_name = "array_access";
    sil_inst->alloc_type = ctx.GetSILType(aa->type_id);  // element type
    sil_inst->setOperand(0, ctx.GetValue(aa->array_id));
    sil_inst->setOperand(1, ctx.GetValue(aa->index_id));
    ctx.emit(std::move(sil_inst));
    ctx.SetValue(inst_id, result);
    return;
  }

  // ArrayElementAddr: emit a GEP to get a pointer to arr[index] for store.
  // Result is an address-type value (pointer to element).
  if (auto aea = inst.TryAs<SemIR::ArrayElementAddr>()) {
    auto elem_sil_type = ctx.GetSILType(aea->type_id);
    auto result = AllocValue(ctx, elem_sil_type.getAddressType());
    auto sil_inst = MakeInst(TinySIL::SILInstKind::BuiltinInst, result);
    sil_inst->builtin_name = "array_element_addr";
    sil_inst->alloc_type = elem_sil_type;
    sil_inst->setOperand(0, ctx.GetValue(aea->array_id));
    sil_inst->setOperand(1, ctx.GetValue(aea->index_id));
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
    // M53: If the captured inst is a VarStorage (alloca), emit a Load so the
    // closure receives the actual value (not the alloca pointer).
    auto outer_inst = sem_ir.insts().Get(capture_ref->captured_inst_id);
    if (outer_inst.Is<SemIR::VarStorage>()) {
      auto addr_val = ctx.GetValue(capture_ref->captured_inst_id);
      if (addr_val.is_valid()) {
        auto result = AllocValue(ctx, ctx.GetSILType(inst.type_id()));
        auto load_inst = MakeInst(TinySIL::SILInstKind::Load, result);
        load_inst->setOperand(0, addr_val);
        ctx.emit(std::move(load_inst));
        ctx.SetValue(inst_id, result);
      }
    } else {
      auto value = ctx.GetValue(capture_ref->captured_inst_id);
      if (value.is_valid()) {
        ctx.SetValue(inst_id, value);
      }
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

  // M38: IntToString — convert Int to String via runtime call.
  if (auto its = inst.TryAs<SemIR::IntToString>()) {
    auto result = EmitBuiltinUnary(ctx, "int_to_string",
                                   its->operand_id, its->type_id);
    ctx.SetValue(inst_id, result);
    return;
  }

  // M81: ThrowValue — set thread-local error + return default.
  if (auto tv = inst.TryAs<SemIR::ThrowValue>()) {
    // Emit error_set(error_value).
    auto error_val = ctx.GetValue(tv->error_id);
    auto set_inst = MakeVoidInst(TinySIL::SILInstKind::BuiltinInst);
    set_inst->builtin_name = "error_set";
    if (error_val.is_valid()) {
      set_inst->operand_list.push_back(error_val);
    } else {
      // Fallback: set error to 1 if we can't get the value.
      set_inst->literal_value = 1;
    }
    ctx.emit(std::move(set_inst));

    // Return default value (0 for integer types).
    auto zero = AllocValue(ctx, ctx.GetSILType(SemIR::ErrorInst::TypeId));
    auto zero_inst = MakeInst(TinySIL::SILInstKind::IntegerLiteral, zero);
    zero_inst->literal_value = 0;
    ctx.emit(std::move(zero_inst));
    auto ret_inst = MakeVoidInst(TinySIL::SILInstKind::ReturnInst);
    ret_inst->setOperand(0, zero);
    ctx.emit(std::move(ret_inst));
    return;
  }

  // M81: ErrorCheck — returns i1 (true if error is pending).
  if (auto ec = inst.TryAs<SemIR::ErrorCheck>()) {
    auto result = AllocValue(ctx, ctx.GetSILType(ec->type_id));
    auto sil_inst = MakeInst(TinySIL::SILInstKind::BuiltinInst, result);
    sil_inst->builtin_name = "error_check";
    ctx.emit(std::move(sil_inst));
    ctx.SetValue(inst_id, result);
    return;
  }

  // M81: ErrorClear — clear the thread-local error slot.
  if (inst.Is<SemIR::ErrorClear>()) {
    auto sil_inst = MakeVoidInst(TinySIL::SILInstKind::BuiltinInst);
    sil_inst->builtin_name = "error_clear";
    ctx.emit(std::move(sil_inst));
    return;
  }

  // M42: DictInit — initialize a dictionary via runtime call.
  if (auto di = inst.TryAs<SemIR::DictInit>()) {
    // Emit as a builtin.  operand_list = interleaved [k0,v0,k1,v1,...].
    // literal_value = number of key-value pairs.
    auto result = AllocValue(ctx, ctx.GetSILType(SemIR::ErrorInst::TypeId)
                                      .getAddressType());
    auto sil_inst = MakeInst(TinySIL::SILInstKind::BuiltinInst, result);
    sil_inst->builtin_name = "dict_init";
    int64_t pair_count = 0;
    if (di->entries_id.has_value() &&
        di->entries_id != SemIR::InstBlockId::Empty) {
      auto entry_ids = sem_ir.inst_blocks().Get(di->entries_id);
      pair_count = static_cast<int64_t>(entry_ids.size() / 2);
      for (auto eid : entry_ids) {
        auto ev = ctx.GetValue(eid);
        if (ev.is_valid()) sil_inst->operand_list.push_back(ev);
      }
    }
    sil_inst->literal_value = pair_count;
    ctx.emit(std::move(sil_inst));
    ctx.SetValue(inst_id, result);
    return;
  }

  // M42: DictAccess — subscript a dictionary.
  if (auto da = inst.TryAs<SemIR::DictAccess>()) {
    auto result = AllocValue(ctx, ctx.GetSILType(da->type_id));
    auto sil_inst = MakeInst(TinySIL::SILInstKind::BuiltinInst, result);
    sil_inst->builtin_name = "dict_access";
    sil_inst->setOperand(0, ctx.GetValue(da->dict_id));
    sil_inst->setOperand(1, ctx.GetValue(da->key_id));
    ctx.emit(std::move(sil_inst));
    ctx.SetValue(inst_id, result);
    return;
  }

  // M91: DictCreate — emit hashmap_create builtin.
  if (auto dc = inst.TryAs<SemIR::DictCreate>()) {
    auto result = AllocValue(ctx, ctx.GetSILType(SemIR::ErrorInst::TypeId)
                                      .getAddressType());
    auto sil_inst = MakeInst(TinySIL::SILInstKind::BuiltinInst, result);
    sil_inst->builtin_name = "hashmap_create";
    // Pack key_type_id and val_type_id into literal_value.
    int64_t key_type_idx = static_cast<int64_t>(dc->type_id.index);
    int64_t val_type_idx = 0;
    if (dc->entries_id.has_value() &&
        dc->entries_id != SemIR::InstBlockId::Empty) {
      auto block = sem_ir.inst_blocks().Get(dc->entries_id);
      if (!block.empty()) {
        val_type_idx = static_cast<int64_t>(block[0].index);
      }
    }
    sil_inst->literal_value = (key_type_idx << 32) | (val_type_idx & 0xFFFFFFFF);
    ctx.emit(std::move(sil_inst));
    ctx.SetValue(inst_id, result);
    return;
  }

  // M91: DictSet — emit hashmap_set builtin.
  if (auto ds = inst.TryAs<SemIR::DictSet>()) {
    auto sil_inst = MakeVoidInst(TinySIL::SILInstKind::BuiltinInst);
    sil_inst->builtin_name = "hashmap_set";
    sil_inst->setOperand(0, ctx.GetValue(ds->dict_id));
    // args_id holds [key_id, val_id].
    if (ds->args_id.has_value() &&
        ds->args_id != SemIR::InstBlockId::Empty) {
      auto args = sem_ir.inst_blocks().Get(ds->args_id);
      if (args.size() >= 1) {
        auto kv = ctx.GetValue(args[0]);
        if (kv.is_valid()) sil_inst->operand_list.push_back(kv);
      }
      if (args.size() >= 2) {
        auto vv = ctx.GetValue(args[1]);
        if (vv.is_valid()) sil_inst->operand_list.push_back(vv);
      }
    }
    // Store key type id in literal_value for hash dispatch in lower.cpp.
    sil_inst->literal_value = 0;
    ctx.emit(std::move(sil_inst));
    return;
  }

  // M91: DictGet — emit hashmap_get builtin.
  if (auto dg = inst.TryAs<SemIR::DictGet>()) {
    auto result = AllocValue(ctx, ctx.GetSILType(dg->type_id));
    auto sil_inst = MakeInst(TinySIL::SILInstKind::BuiltinInst, result);
    sil_inst->builtin_name = "hashmap_get";
    sil_inst->setOperand(0, ctx.GetValue(dg->dict_id));
    sil_inst->setOperand(1, ctx.GetValue(dg->key_id));
    // Store val type id in literal_value.
    sil_inst->literal_value = static_cast<int64_t>(dg->type_id.index);
    ctx.emit(std::move(sil_inst));
    ctx.SetValue(inst_id, result);
    return;
  }

  // M91: DictCount — emit hashmap_count builtin.
  if (auto dc = inst.TryAs<SemIR::DictCount>()) {
    auto result = AllocValue(ctx, ctx.GetSILType(dc->type_id));
    auto sil_inst = MakeInst(TinySIL::SILInstKind::BuiltinInst, result);
    sil_inst->builtin_name = "hashmap_count";
    sil_inst->setOperand(0, ctx.GetValue(dc->dict_id));
    ctx.emit(std::move(sil_inst));
    ctx.SetValue(inst_id, result);
    return;
  }

  // M91: DictContains — emit hashmap_contains builtin.
  if (auto dc = inst.TryAs<SemIR::DictContains>()) {
    auto result = AllocValue(ctx, ctx.GetSILType(dc->type_id));
    auto sil_inst = MakeInst(TinySIL::SILInstKind::BuiltinInst, result);
    sil_inst->builtin_name = "hashmap_contains";
    sil_inst->setOperand(0, ctx.GetValue(dc->dict_id));
    sil_inst->setOperand(1, ctx.GetValue(dc->key_id));
    ctx.emit(std::move(sil_inst));
    ctx.SetValue(inst_id, result);
    return;
  }

  // M91: DictRemove — emit hashmap_remove builtin.
  if (auto dr = inst.TryAs<SemIR::DictRemove>()) {
    auto sil_inst = MakeVoidInst(TinySIL::SILInstKind::BuiltinInst);
    sil_inst->builtin_name = "hashmap_remove";
    sil_inst->setOperand(0, ctx.GetValue(dr->dict_id));
    sil_inst->setOperand(1, ctx.GetValue(dr->key_id));
    ctx.emit(std::move(sil_inst));
    return;
  }

  // M91: DictMethodRef — should be resolved by HandleCallExpr; skip silently.
  if (inst.Is<SemIR::DictMethodRef>()) {
    return;
  }

  // M91: SetCreate — emit hashset_create builtin.
  if (auto sc = inst.TryAs<SemIR::SetCreate>()) {
    auto result = AllocValue(ctx, ctx.GetSILType(SemIR::ErrorInst::TypeId)
                                      .getAddressType());
    auto sil_inst = MakeInst(TinySIL::SILInstKind::BuiltinInst, result);
    sil_inst->builtin_name = "hashset_create";
    sil_inst->literal_value = static_cast<int64_t>(sc->type_id.index);
    ctx.emit(std::move(sil_inst));
    ctx.SetValue(inst_id, result);
    return;
  }

  // M91: SetInsert — emit hashset_insert builtin.
  if (auto si = inst.TryAs<SemIR::SetInsert>()) {
    auto sil_inst = MakeVoidInst(TinySIL::SILInstKind::BuiltinInst);
    sil_inst->builtin_name = "hashset_insert";
    sil_inst->setOperand(0, ctx.GetValue(si->set_id));
    sil_inst->setOperand(1, ctx.GetValue(si->elem_id));
    ctx.emit(std::move(sil_inst));
    return;
  }

  // M91: SetContains — emit hashset_contains builtin.
  if (auto sc = inst.TryAs<SemIR::SetContains>()) {
    auto result = AllocValue(ctx, ctx.GetSILType(sc->type_id));
    auto sil_inst = MakeInst(TinySIL::SILInstKind::BuiltinInst, result);
    sil_inst->builtin_name = "hashset_contains";
    sil_inst->setOperand(0, ctx.GetValue(sc->set_id));
    sil_inst->setOperand(1, ctx.GetValue(sc->elem_id));
    ctx.emit(std::move(sil_inst));
    ctx.SetValue(inst_id, result);
    return;
  }

  // M91: SetCount — emit hashset_count builtin.
  if (auto sc = inst.TryAs<SemIR::SetCount>()) {
    auto result = AllocValue(ctx, ctx.GetSILType(sc->type_id));
    auto sil_inst = MakeInst(TinySIL::SILInstKind::BuiltinInst, result);
    sil_inst->builtin_name = "hashset_count";
    sil_inst->setOperand(0, ctx.GetValue(sc->set_id));
    ctx.emit(std::move(sil_inst));
    ctx.SetValue(inst_id, result);
    return;
  }

  // M91: SetRemove — emit hashset_remove builtin.
  if (auto sr = inst.TryAs<SemIR::SetRemove>()) {
    auto sil_inst = MakeVoidInst(TinySIL::SILInstKind::BuiltinInst);
    sil_inst->builtin_name = "hashset_remove";
    sil_inst->setOperand(0, ctx.GetValue(sr->set_id));
    sil_inst->setOperand(1, ctx.GetValue(sr->elem_id));
    ctx.emit(std::move(sil_inst));
    return;
  }

  // M91: SetMethodRef — should be resolved by HandleCallExpr; skip silently.
  if (inst.Is<SemIR::SetMethodRef>()) {
    return;
  }

  // M44: PrintValue — emit BuiltinInst("print_int" or "print_string").
  // Use MakeVoidInst so DCE doesn't eliminate the side-effecting call.
  if (auto pv = inst.TryAs<SemIR::PrintValue>()) {
    auto sil_inst = MakeVoidInst(TinySIL::SILInstKind::BuiltinInst);
    if (pv->arg_id.has_value()) {
      auto arg_val = ctx.GetValue(pv->arg_id);
      // Detect string type via SemIR type_id.
      bool is_string = false;
      auto arg_semtype = sem_ir.insts().Get(pv->arg_id).type_id();
      if (arg_semtype.has_value()) {
        auto string_ti = sem_ir.types().GetTypeInstId(arg_semtype);
        if (string_ti.has_value() &&
            sem_ir.insts().Get(string_ti).Is<SemIR::StringType>()) {
          is_string = true;
        }
      }
      sil_inst->builtin_name = is_string ? "print_string" : "print_int";
      if (arg_val.is_valid()) {
        sil_inst->setOperand(0, arg_val);
      }
    } else {
      sil_inst->builtin_name = "print_int";
    }
    ctx.emit(std::move(sil_inst));
    // No value binding needed (void result).
    return;
  }

  // M92: ReadLine — emit readline builtin.
  if (auto rl = inst.TryAs<SemIR::ReadLine>()) {
    auto result = AllocValue(ctx, ctx.GetSILType(rl->type_id));
    auto sil_inst = MakeInst(TinySIL::SILInstKind::BuiltinInst, result);
    sil_inst->builtin_name = "readline";
    ctx.emit(std::move(sil_inst));
    ctx.SetValue(inst_id, result);
    return;
  }

  // M92: FileGetCwd — emit file_getcwd builtin.
  if (auto gc = inst.TryAs<SemIR::FileGetCwd>()) {
    auto result = AllocValue(ctx, ctx.GetSILType(gc->type_id));
    auto sil_inst = MakeInst(TinySIL::SILInstKind::BuiltinInst, result);
    sil_inst->builtin_name = "file_getcwd";
    ctx.emit(std::move(sil_inst));
    ctx.SetValue(inst_id, result);
    return;
  }

  // M92: FileReadAll — emit file_read_all builtin.
  if (auto fr = inst.TryAs<SemIR::FileReadAll>()) {
    auto result = AllocValue(ctx, ctx.GetSILType(fr->type_id));
    auto sil_inst = MakeInst(TinySIL::SILInstKind::BuiltinInst, result);
    sil_inst->builtin_name = "file_read_all";
    sil_inst->setOperand(0, ctx.GetValue(fr->path_id));
    ctx.emit(std::move(sil_inst));
    ctx.SetValue(inst_id, result);
    return;
  }

  // M92: FileExists — emit file_exists builtin.
  if (auto fe = inst.TryAs<SemIR::FileExists>()) {
    auto result = AllocValue(ctx, ctx.GetSILType(fe->type_id));
    auto sil_inst = MakeInst(TinySIL::SILInstKind::BuiltinInst, result);
    sil_inst->builtin_name = "file_exists";
    sil_inst->setOperand(0, ctx.GetValue(fe->path_id));
    ctx.emit(std::move(sil_inst));
    ctx.SetValue(inst_id, result);
    return;
  }

  // M92: FileRemove — emit file_remove builtin.
  if (auto fr = inst.TryAs<SemIR::FileRemove>()) {
    auto result = AllocValue(ctx, ctx.GetSILType(fr->type_id));
    auto sil_inst = MakeInst(TinySIL::SILInstKind::BuiltinInst, result);
    sil_inst->builtin_name = "file_remove";
    sil_inst->setOperand(0, ctx.GetValue(fr->path_id));
    ctx.emit(std::move(sil_inst));
    ctx.SetValue(inst_id, result);
    return;
  }

  // M92: FileWriteAll — emit file_write_all builtin.
  if (auto fw = inst.TryAs<SemIR::FileWriteAll>()) {
    auto result = AllocValue(ctx, ctx.GetSILType(fw->type_id));
    auto sil_inst = MakeInst(TinySIL::SILInstKind::BuiltinInst, result);
    sil_inst->builtin_name = "file_write_all";
    sil_inst->setOperand(0, ctx.GetValue(fw->path_id));
    sil_inst->setOperand(1, ctx.GetValue(fw->contents_id));
    ctx.emit(std::move(sil_inst));
    ctx.SetValue(inst_id, result);
    return;
  }

  // M92: FileAppendAll — emit file_append_all builtin.
  if (auto fa = inst.TryAs<SemIR::FileAppendAll>()) {
    auto result = AllocValue(ctx, ctx.GetSILType(fa->type_id));
    auto sil_inst = MakeInst(TinySIL::SILInstKind::BuiltinInst, result);
    sil_inst->builtin_name = "file_append_all";
    sil_inst->setOperand(0, ctx.GetValue(fa->path_id));
    sil_inst->setOperand(1, ctx.GetValue(fa->contents_id));
    ctx.emit(std::move(sil_inst));
    ctx.SetValue(inst_id, result);
    return;
  }

  // M93: ProcessGetArgs — emit process_get_args builtin.
  if (auto ga = inst.TryAs<SemIR::ProcessGetArgs>()) {
    auto result = AllocValue(ctx, ctx.GetSILType(ga->type_id));
    auto sil_inst = MakeInst(TinySIL::SILInstKind::BuiltinInst, result);
    sil_inst->builtin_name = "process_get_args";
    ctx.emit(std::move(sil_inst));
    ctx.SetValue(inst_id, result);
    return;
  }

  // M93: ProcessExit — emit process_exit builtin.
  if (auto pe = inst.TryAs<SemIR::ProcessExit>()) {
    auto result = AllocValue(ctx, ctx.GetSILType(pe->type_id));
    auto sil_inst = MakeInst(TinySIL::SILInstKind::BuiltinInst, result);
    sil_inst->builtin_name = "process_exit";
    sil_inst->setOperand(0, ctx.GetValue(pe->code_id));
    ctx.emit(std::move(sil_inst));
    ctx.SetValue(inst_id, result);
    return;
  }

  // M93: EnvGet — emit env_get builtin.
  if (auto eg = inst.TryAs<SemIR::EnvGet>()) {
    auto result = AllocValue(ctx, ctx.GetSILType(eg->type_id));
    auto sil_inst = MakeInst(TinySIL::SILInstKind::BuiltinInst, result);
    sil_inst->builtin_name = "env_get";
    sil_inst->setOperand(0, ctx.GetValue(eg->key_id));
    ctx.emit(std::move(sil_inst));
    ctx.SetValue(inst_id, result);
    return;
  }

  // M93: EnvSet — emit env_set builtin.
  if (auto es = inst.TryAs<SemIR::EnvSet>()) {
    auto result = AllocValue(ctx, ctx.GetSILType(es->type_id));
    auto sil_inst = MakeInst(TinySIL::SILInstKind::BuiltinInst, result);
    sil_inst->builtin_name = "env_set";
    sil_inst->setOperand(0, ctx.GetValue(es->key_id));
    sil_inst->setOperand(1, ctx.GetValue(es->value_id));
    ctx.emit(std::move(sil_inst));
    ctx.SetValue(inst_id, result);
    return;
  }

  // M93: FsMkdir — emit fs_mkdir builtin.
  if (auto fm = inst.TryAs<SemIR::FsMkdir>()) {
    auto result = AllocValue(ctx, ctx.GetSILType(fm->type_id));
    auto sil_inst = MakeInst(TinySIL::SILInstKind::BuiltinInst, result);
    sil_inst->builtin_name = "fs_mkdir";
    sil_inst->setOperand(0, ctx.GetValue(fm->path_id));
    ctx.emit(std::move(sil_inst));
    ctx.SetValue(inst_id, result);
    return;
  }

  // M93: FsListDir — emit fs_listdir builtin.
  if (auto fl = inst.TryAs<SemIR::FsListDir>()) {
    auto result = AllocValue(ctx, ctx.GetSILType(fl->type_id));
    auto sil_inst = MakeInst(TinySIL::SILInstKind::BuiltinInst, result);
    sil_inst->builtin_name = "fs_listdir";
    sil_inst->setOperand(0, ctx.GetValue(fl->path_id));
    ctx.emit(std::move(sil_inst));
    ctx.SetValue(inst_id, result);
    return;
  }

  // M93: FsIsDir — emit fs_is_dir builtin.
  if (auto fi = inst.TryAs<SemIR::FsIsDir>()) {
    auto result = AllocValue(ctx, ctx.GetSILType(fi->type_id));
    auto sil_inst = MakeInst(TinySIL::SILInstKind::BuiltinInst, result);
    sil_inst->builtin_name = "fs_is_dir";
    sil_inst->setOperand(0, ctx.GetValue(fi->path_id));
    ctx.emit(std::move(sil_inst));
    ctx.SetValue(inst_id, result);
    return;
  }

  // M93: FsCopy — emit fs_copy builtin.
  if (auto fc = inst.TryAs<SemIR::FsCopy>()) {
    auto result = AllocValue(ctx, ctx.GetSILType(fc->type_id));
    auto sil_inst = MakeInst(TinySIL::SILInstKind::BuiltinInst, result);
    sil_inst->builtin_name = "fs_copy";
    sil_inst->setOperand(0, ctx.GetValue(fc->src_id));
    sil_inst->setOperand(1, ctx.GetValue(fc->dst_id));
    ctx.emit(std::move(sil_inst));
    ctx.SetValue(inst_id, result);
    return;
  }

  // M94: TcpConnect — emit tcp_connect builtin.
  if (auto tc = inst.TryAs<SemIR::TcpConnect>()) {
    auto result = AllocValue(ctx, ctx.GetSILType(tc->type_id));
    auto sil_inst = MakeInst(TinySIL::SILInstKind::BuiltinInst, result);
    sil_inst->builtin_name = "tcp_connect";
    sil_inst->setOperand(0, ctx.GetValue(tc->host_id));
    sil_inst->setOperand(1, ctx.GetValue(tc->port_id));
    ctx.emit(std::move(sil_inst));
    ctx.SetValue(inst_id, result);
    return;
  }

  // M94: TcpListen — emit tcp_listen builtin.
  if (auto tl = inst.TryAs<SemIR::TcpListen>()) {
    auto result = AllocValue(ctx, ctx.GetSILType(tl->type_id));
    auto sil_inst = MakeInst(TinySIL::SILInstKind::BuiltinInst, result);
    sil_inst->builtin_name = "tcp_listen";
    sil_inst->setOperand(0, ctx.GetValue(tl->port_id));
    ctx.emit(std::move(sil_inst));
    ctx.SetValue(inst_id, result);
    return;
  }

  // M94: TcpAccept — emit tcp_accept builtin.
  if (auto ta = inst.TryAs<SemIR::TcpAccept>()) {
    auto result = AllocValue(ctx, ctx.GetSILType(ta->type_id));
    auto sil_inst = MakeInst(TinySIL::SILInstKind::BuiltinInst, result);
    sil_inst->builtin_name = "tcp_accept";
    sil_inst->setOperand(0, ctx.GetValue(ta->fd_id));
    ctx.emit(std::move(sil_inst));
    ctx.SetValue(inst_id, result);
    return;
  }

  // M94: TcpRead — emit tcp_read builtin.
  if (auto tr = inst.TryAs<SemIR::TcpRead>()) {
    auto result = AllocValue(ctx, ctx.GetSILType(tr->type_id));
    auto sil_inst = MakeInst(TinySIL::SILInstKind::BuiltinInst, result);
    sil_inst->builtin_name = "tcp_read";
    sil_inst->setOperand(0, ctx.GetValue(tr->fd_id));
    sil_inst->setOperand(1, ctx.GetValue(tr->maxlen_id));
    ctx.emit(std::move(sil_inst));
    ctx.SetValue(inst_id, result);
    return;
  }

  // M94: TcpWrite — emit tcp_write builtin.
  if (auto tw = inst.TryAs<SemIR::TcpWrite>()) {
    auto result = AllocValue(ctx, ctx.GetSILType(tw->type_id));
    auto sil_inst = MakeInst(TinySIL::SILInstKind::BuiltinInst, result);
    sil_inst->builtin_name = "tcp_write";
    sil_inst->setOperand(0, ctx.GetValue(tw->fd_id));
    sil_inst->setOperand(1, ctx.GetValue(tw->data_id));
    ctx.emit(std::move(sil_inst));
    ctx.SetValue(inst_id, result);
    return;
  }

  // M94: TcpClose — emit tcp_close builtin.
  if (auto tcl = inst.TryAs<SemIR::TcpClose>()) {
    auto result = AllocValue(ctx, ctx.GetSILType(tcl->type_id));
    auto sil_inst = MakeInst(TinySIL::SILInstKind::BuiltinInst, result);
    sil_inst->builtin_name = "tcp_close";
    sil_inst->setOperand(0, ctx.GetValue(tcl->fd_id));
    ctx.emit(std::move(sil_inst));
    ctx.SetValue(inst_id, result);
    return;
  }

  // M45: StringEq — call __tinyswift_string_eq at runtime.
  if (auto seq = inst.TryAs<SemIR::StringEq>()) {
    auto result = AllocValue(ctx, ctx.GetSILType(seq->type_id));
    auto sil_inst = MakeInst(TinySIL::SILInstKind::BuiltinInst, result);
    sil_inst->builtin_name = "string_eq";
    sil_inst->setOperand(0, ctx.GetValue(seq->lhs_id));
    sil_inst->setOperand(1, ctx.GetValue(seq->rhs_id));
    ctx.emit(std::move(sil_inst));
    ctx.SetValue(inst_id, result);
    return;
  }

  // M45: StringNeq — call __tinyswift_string_eq then invert.
  if (auto sneq = inst.TryAs<SemIR::StringNeq>()) {
    auto result = AllocValue(ctx, ctx.GetSILType(sneq->type_id));
    auto sil_inst = MakeInst(TinySIL::SILInstKind::BuiltinInst, result);
    sil_inst->builtin_name = "string_neq";
    sil_inst->setOperand(0, ctx.GetValue(sneq->lhs_id));
    sil_inst->setOperand(1, ctx.GetValue(sneq->rhs_id));
    ctx.emit(std::move(sil_inst));
    ctx.SetValue(inst_id, result);
    return;
  }

  // M49: StringLen — call __tinyswift_string_len(s) -> Int.
  if (auto sl = inst.TryAs<SemIR::StringLen>()) {
    auto result = AllocValue(ctx, ctx.GetSILType(sl->type_id));
    auto sil_inst = MakeInst(TinySIL::SILInstKind::BuiltinInst, result);
    sil_inst->builtin_name = "string_len";
    sil_inst->setOperand(0, ctx.GetValue(sl->operand_id));
    ctx.emit(std::move(sil_inst));
    ctx.SetValue(inst_id, result);
    return;
  }

  // M61: StringUppercased — call __tinyswift_string_uppercased(s) -> String.
  if (auto su = inst.TryAs<SemIR::StringUppercased>()) {
    auto result = AllocValue(ctx, ctx.GetSILType(su->type_id));
    auto sil_inst = MakeInst(TinySIL::SILInstKind::BuiltinInst, result);
    sil_inst->builtin_name = "string_uppercased";
    sil_inst->setOperand(0, ctx.GetValue(su->str_id));
    ctx.emit(std::move(sil_inst));
    ctx.SetValue(inst_id, result);
    return;
  }

  // M61: StringLowercased — call __tinyswift_string_lowercased(s) -> String.
  if (auto sl = inst.TryAs<SemIR::StringLowercased>()) {
    auto result = AllocValue(ctx, ctx.GetSILType(sl->type_id));
    auto sil_inst = MakeInst(TinySIL::SILInstKind::BuiltinInst, result);
    sil_inst->builtin_name = "string_lowercased";
    sil_inst->setOperand(0, ctx.GetValue(sl->str_id));
    ctx.emit(std::move(sil_inst));
    ctx.SetValue(inst_id, result);
    return;
  }

  // M61: StringTrimmed — call __tinyswift_string_trimmed(s) -> String.
  if (auto st = inst.TryAs<SemIR::StringTrimmed>()) {
    auto result = AllocValue(ctx, ctx.GetSILType(st->type_id));
    auto sil_inst = MakeInst(TinySIL::SILInstKind::BuiltinInst, result);
    sil_inst->builtin_name = "string_trimmed";
    sil_inst->setOperand(0, ctx.GetValue(st->str_id));
    ctx.emit(std::move(sil_inst));
    ctx.SetValue(inst_id, result);
    return;
  }

  // M61: StringHasPrefix — call __tinyswift_string_has_prefix(s, p) -> Bool.
  if (auto shp = inst.TryAs<SemIR::StringHasPrefix>()) {
    auto result = AllocValue(ctx, ctx.GetSILType(shp->type_id));
    auto sil_inst = MakeInst(TinySIL::SILInstKind::BuiltinInst, result);
    sil_inst->builtin_name = "string_has_prefix";
    sil_inst->setOperand(0, ctx.GetValue(shp->str_id));
    sil_inst->setOperand(1, ctx.GetValue(shp->arg_id));
    ctx.emit(std::move(sil_inst));
    ctx.SetValue(inst_id, result);
    return;
  }

  // M61: StringHasSuffix — call __tinyswift_string_has_suffix(s, p) -> Bool.
  if (auto shs = inst.TryAs<SemIR::StringHasSuffix>()) {
    auto result = AllocValue(ctx, ctx.GetSILType(shs->type_id));
    auto sil_inst = MakeInst(TinySIL::SILInstKind::BuiltinInst, result);
    sil_inst->builtin_name = "string_has_suffix";
    sil_inst->setOperand(0, ctx.GetValue(shs->str_id));
    sil_inst->setOperand(1, ctx.GetValue(shs->arg_id));
    ctx.emit(std::move(sil_inst));
    ctx.SetValue(inst_id, result);
    return;
  }

  // M61: StringContains — call __tinyswift_string_contains(s, sub) -> Bool.
  if (auto sc = inst.TryAs<SemIR::StringContains>()) {
    auto result = AllocValue(ctx, ctx.GetSILType(sc->type_id));
    auto sil_inst = MakeInst(TinySIL::SILInstKind::BuiltinInst, result);
    sil_inst->builtin_name = "string_contains";
    sil_inst->setOperand(0, ctx.GetValue(sc->str_id));
    sil_inst->setOperand(1, ctx.GetValue(sc->arg_id));
    ctx.emit(std::move(sil_inst));
    ctx.SetValue(inst_id, result);
    return;
  }

  // M61: StringMethodRef — pending marker, should be resolved before sil_gen.
  if (inst.Is<SemIR::StringMethodRef>()) { return; }

  // M65/M90: DynamicArrayInit — call __tinyswift_dynarray_create_generic(elem_size).
  // Use address-type result (same opaque-pointer pattern as DictInit).
  // Store element TypeId in literal_value for lower.cpp to compute elem_size.
  if (auto dai = inst.TryAs<SemIR::DynamicArrayInit>()) {
    auto result = AllocValue(ctx, ctx.GetSILType(SemIR::ErrorInst::TypeId)
                                      .getAddressType());
    auto sil_inst = MakeInst(TinySIL::SILInstKind::BuiltinInst, result);
    sil_inst->builtin_name = "dynarray_create";
    sil_inst->literal_value = static_cast<int64_t>(dai->type_id.index);
    ctx.emit(std::move(sil_inst));
    ctx.SetValue(inst_id, result);
    return;
  }

  // M65/M90: DynamicArrayAppend — call __tinyswift_dynarray_append(arr, &val).
  if (auto daa = inst.TryAs<SemIR::DynamicArrayAppend>()) {
    auto result = AllocValue(ctx, ctx.GetSILType(daa->type_id));
    auto sil_inst = MakeInst(TinySIL::SILInstKind::BuiltinInst, result);
    sil_inst->builtin_name = "dynarray_append";
    sil_inst->setOperand(0, ctx.GetValue(daa->array_id));
    sil_inst->setOperand(1, ctx.GetValue(daa->elem_id));
    ctx.emit(std::move(sil_inst));
    ctx.SetValue(inst_id, result);
    return;
  }

  // M65/M90: DynamicArrayCount — call __tinyswift_dynarray_count(arr) -> Int.
  if (auto dac = inst.TryAs<SemIR::DynamicArrayCount>()) {
    auto result = AllocValue(ctx, ctx.GetSILType(dac->type_id));
    auto sil_inst = MakeInst(TinySIL::SILInstKind::BuiltinInst, result);
    sil_inst->builtin_name = "dynarray_count";
    sil_inst->setOperand(0, ctx.GetValue(dac->array_id));
    ctx.emit(std::move(sil_inst));
    ctx.SetValue(inst_id, result);
    return;
  }

  // M65/M90: DynamicArrayAccess — call __tinyswift_dynarray_get(arr, idx) -> elem.
  // Result type carries the element type for lower.cpp to load correctly.
  if (auto dax = inst.TryAs<SemIR::DynamicArrayAccess>()) {
    auto result = AllocValue(ctx, ctx.GetSILType(dax->type_id));
    auto sil_inst = MakeInst(TinySIL::SILInstKind::BuiltinInst, result);
    sil_inst->builtin_name = "dynarray_access";
    sil_inst->setOperand(0, ctx.GetValue(dax->array_id));
    sil_inst->setOperand(1, ctx.GetValue(dax->index_id));
    // Store element type_id in literal_value so lower.cpp can determine load type.
    sil_inst->literal_value = static_cast<int64_t>(dax->type_id.index);
    ctx.emit(std::move(sil_inst));
    ctx.SetValue(inst_id, result);
    return;
  }

  // M65: DynamicArrayMethodRef — pending marker, should be resolved before sil_gen.
  if (inst.Is<SemIR::DynamicArrayMethodRef>()) { return; }

  // M77: UnsafePtrAllocate — call malloc(capacity * elem_size).
  if (auto upa = inst.TryAs<SemIR::UnsafePtrAllocate>()) {
    auto result = AllocValue(ctx, ctx.GetSILType(upa->type_id).getAddressType());
    auto sil_inst = MakeInst(TinySIL::SILInstKind::BuiltinInst, result);
    sil_inst->builtin_name = "unsafe_ptr_allocate";
    sil_inst->setOperand(0, ctx.GetValue(upa->capacity_id));
    sil_inst->setOperand(1, ctx.GetValue(upa->elem_size_id));
    ctx.emit(std::move(sil_inst));
    ctx.SetValue(inst_id, result);
    return;
  }

  // M77: UnsafePtrDeallocate — call free(ptr).
  if (auto upd = inst.TryAs<SemIR::UnsafePtrDeallocate>()) {
    auto result = AllocValue(ctx, ctx.GetSILType(upd->type_id));
    auto sil_inst = MakeInst(TinySIL::SILInstKind::BuiltinInst, result);
    sil_inst->builtin_name = "unsafe_ptr_deallocate";
    sil_inst->setOperand(0, ctx.GetValue(upd->ptr_id));
    ctx.emit(std::move(sil_inst));
    ctx.SetValue(inst_id, result);
    return;
  }

  // M77: UnsafePtrSubscript — GEP + load.
  if (auto ups = inst.TryAs<SemIR::UnsafePtrSubscript>()) {
    auto result = AllocValue(ctx, ctx.GetSILType(ups->type_id));
    auto sil_inst = MakeInst(TinySIL::SILInstKind::BuiltinInst, result);
    sil_inst->builtin_name = "unsafe_ptr_subscript";
    sil_inst->setOperand(0, ctx.GetValue(ups->ptr_id));
    sil_inst->setOperand(1, ctx.GetValue(ups->index_id));
    ctx.emit(std::move(sil_inst));
    ctx.SetValue(inst_id, result);
    return;
  }

  // M77: UnsafePtrSubscriptAddr — GEP for store (address).
  if (auto upsa = inst.TryAs<SemIR::UnsafePtrSubscriptAddr>()) {
    auto result = AllocValue(ctx, ctx.GetSILType(upsa->type_id).getAddressType());
    auto sil_inst = MakeInst(TinySIL::SILInstKind::BuiltinInst, result);
    sil_inst->builtin_name = "unsafe_ptr_subscript_addr";
    sil_inst->setOperand(0, ctx.GetValue(upsa->ptr_id));
    sil_inst->setOperand(1, ctx.GetValue(upsa->index_id));
    ctx.emit(std::move(sil_inst));
    ctx.SetValue(inst_id, result);
    return;
  }

  // M77: UnsafePtrMethodRef — pending marker, resolved in check phase.
  if (inst.Is<SemIR::UnsafePtrMethodRef>()) { return; }

  // M78: AllocClass — call __tinyswift_alloc(size) then GEP+store fields.
  if (auto ac = inst.TryAs<SemIR::AllocClass>()) {
    auto result = AllocValue(ctx, ctx.GetSILType(ac->type_id).getAddressType());
    auto sil_inst = MakeInst(TinySIL::SILInstKind::BuiltinInst, result);
    sil_inst->builtin_name = "alloc_class";
    // Pass field values as operand_list.
    if (ac->args_id.has_value() &&
        ac->args_id != SemIR::InstBlockId::Empty) {
      auto field_ids = ctx.sem_ir().inst_blocks().Get(ac->args_id);
      for (auto field_id : field_ids) {
        auto fv = ctx.GetValue(field_id);
        if (fv.is_valid()) {
          sil_inst->operand_list.push_back(fv);
        }
      }
    }
    // Store the class type_id as literal_value for lowering to compute size.
    sil_inst->literal_value = static_cast<int64_t>(ac->type_id.index);
    ctx.emit(std::move(sil_inst));
    ctx.SetValue(inst_id, result);
    return;
  }

  // M78: Retain — call __tinyswift_retain(obj).
  if (auto ret = inst.TryAs<SemIR::Retain>()) {
    auto result = AllocValue(ctx, ctx.GetSILType(ret->type_id));
    auto sil_inst = MakeInst(TinySIL::SILInstKind::BuiltinInst, result);
    sil_inst->builtin_name = "retain";
    sil_inst->setOperand(0, ctx.GetValue(ret->value_id));
    ctx.emit(std::move(sil_inst));
    ctx.SetValue(inst_id, result);
    return;
  }

  // M78: Release — call __tinyswift_release(obj, deinit_fn_ptr_or_null).
  // M97: Use "release_cycle" for cycle-capable types.
  if (auto rel = inst.TryAs<SemIR::Release>()) {
    auto result = AllocValue(ctx, ctx.GetSILType(rel->type_id));
    auto sil_inst = MakeInst(TinySIL::SILInstKind::BuiltinInst, result);
    // Check if the released value's type is cycle-capable.
    auto val_inst = sem_ir.insts().Get(rel->value_id);
    auto val_type_id = val_inst.type_id();
    if (val_type_id.has_value() && sem_ir.IsCycleCapableType(val_type_id)) {
      sil_inst->builtin_name = "release_cycle";
    } else {
      sil_inst->builtin_name = "release";
    }
    sil_inst->setOperand(0, ctx.GetValue(rel->value_id));
    // deinit_id: if set, emit a function_ref and pass as operand 1.
    if (rel->deinit_id.has_value()) {
      auto deinit_inst = sem_ir.insts().Get(rel->deinit_id);
      if (auto fn_decl = deinit_inst.TryAs<SemIR::FunctionDecl>()) {
        auto& function = sem_ir.functions().Get(fn_decl->function_id);
        std::string func_name = ctx.GetFunctionName(function);
        auto fn_ptr_type = TinySIL::SILType{
            .type_index = rel->type_id.index, .is_address = false};
        auto fn_ref_val = AllocValue(ctx, fn_ptr_type);
        auto fn_ref_inst =
            MakeInst(TinySIL::SILInstKind::FunctionRef, fn_ref_val);
        fn_ref_inst->function_name = func_name;
        ctx.emit(std::move(fn_ref_inst));
        sil_inst->setOperand(1, fn_ref_val);
      }
    }
    ctx.emit(std::move(sil_inst));
    ctx.SetValue(inst_id, result);
    return;
  }

  // M40: InoutParam — like ValueParam, but tracks as pointer.
  if (auto inout_param = inst.TryAs<SemIR::InoutParam>()) {
    if (!ctx.HasValue(inst_id)) {
      // Pointer type for inout: address-typed SIL value.
      auto ptr_type = ctx.GetSILType(inout_param->type_id).getAddressType();
      auto result = AllocValue(ctx, ptr_type);
      ctx.SetValue(inst_id, result);
    }
    return;
  }

  // M40: AddressOf — pass the raw VarStorage alloca pointer.
  if (auto addr_of = inst.TryAs<SemIR::AddressOf>()) {
    // The operand_id is a VarStorage — its sil_gen value is the alloca ptr.
    auto ptr_val = ctx.GetValue(addr_of->operand_id);
    if (ptr_val.is_valid()) {
      ctx.SetValue(inst_id, ptr_val);
    }
    return;
  }

  // Unknown instruction kind — fatal error to catch missing handlers early.
  llvm::errs() << "FATAL: unhandled SemIR instruction kind in sil_gen"
               << " (inst_id=" << inst_id.index << ")\n";
  llvm::report_fatal_error("unhandled SemIR inst kind in sil_gen EmitInst");
}

// Emits TinySIL for a function body.
auto EmitFunctionBody(Context& ctx, SemIR::FunctionId func_id,
                      const SemIR::Function& function) -> void {
  auto& sem_ir = ctx.sem_ir();
  std::string func_name = ctx.GetFunctionName(function);

  auto* sil_fn = ctx.sil_module().createFunction(func_name);
  sil_fn->sem_ir_function_id = func_id.index;
  ctx.set_current_function(sil_fn);

  // Build the SIL function type (before early-return for declarations so
  // extern/cdecl functions get correct param/return types).
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
      } else if (auto inout_param = param_inst.TryAs<SemIR::InoutParam>()) {
        // M40: inout params are passed as pointer (address type).
        auto param_type = ctx.GetSILType(inout_param->type_id).getAddressType();
        sil_fn->type.param_types.push_back(param_type);
        sil_fn->type.param_conventions.push_back(TinySIL::Ownership::Owned);
      }
    }
  }

  if (function.body_block_ids.empty()) {
    sil_fn->is_declaration = true;
    ctx.clearFunctionState();
    return;
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
      } else if (auto inout_param = param_inst.TryAs<SemIR::InoutParam>()) {
        // M40: inout param is a pointer argument.
        auto ptr_type = ctx.GetSILType(inout_param->type_id).getAddressType();
        auto arg_val = AllocValue(ctx, ptr_type);
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
    if (!bb->is_terminated()) {
      if (sil_fn->type.is_void_return) {
        // Void function with no explicit return: emit a void return.
        auto ret_inst = MakeVoidInst(TinySIL::SILInstKind::ReturnInst);
        bb->addInst(std::move(ret_inst));
      } else {
        auto unreachable = MakeVoidInst(TinySIL::SILInstKind::Unreachable);
        bb->addInst(std::move(unreachable));
      }
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
