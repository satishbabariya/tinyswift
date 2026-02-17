// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "toolchain/lower/context.h"
#include "toolchain/sem_ir/typed_insts.h"

namespace TinySwift::Lower {

auto LowerInst(Context& context, SemIR::InstId inst_id) -> void {
  auto& sem_ir = context.sem_ir();
  auto inst = sem_ir.insts().Get(inst_id);

  // Skip instructions that are not lowered.
  if (!inst.kind().ir_name().empty() &&
      inst.kind().terminator_kind() == SemIR::TerminatorKind::NotTerminator) {
    // Check is_lowered via the definition info.
  }

  switch (inst.kind()) {
    // -----------------------------------------------------------------------
    // Non-lowered / no-op instructions
    // -----------------------------------------------------------------------
    case SemIR::InstKind::ErrorInst:
    case SemIR::InstKind::ImportDecl:
    case SemIR::InstKind::FunctionDecl:
    case SemIR::InstKind::ValueBindingPattern:
    case SemIR::InstKind::ValueParamPattern:
    case SemIR::InstKind::NameBindingDecl:
    case SemIR::InstKind::Namespace:
      // These are compile-time only or pattern metadata; nothing to lower.
      break;

    // Type instructions are not runtime values.
    case SemIR::InstKind::BoolType:
    case SemIR::InstKind::IntType:
    case SemIR::InstKind::IntLiteralType:
    case SemIR::InstKind::FunctionType:
    case SemIR::InstKind::PointerType:
    case SemIR::InstKind::TypeType:
    case SemIR::InstKind::NamespaceType:
    case SemIR::InstKind::StringType:
    case SemIR::InstKind::FloatType:
    case SemIR::InstKind::DoubleType:
    case SemIR::InstKind::OptionalType:
    case SemIR::InstKind::StructType:
    case SemIR::InstKind::ClassType:
      break;

    // -----------------------------------------------------------------------
    // Literals and constants
    // -----------------------------------------------------------------------
    case SemIR::InstKind::BoolLiteral: {
      auto bool_lit = inst.As<SemIR::BoolLiteral>();
      auto* value = llvm::ConstantInt::get(
          llvm::Type::getInt1Ty(context.llvm_context()),
          bool_lit.value.ToBool() ? 1 : 0);
      context.SetLocal(inst_id, value);
      break;
    }

    case SemIR::InstKind::IntValue: {
      auto int_val = inst.As<SemIR::IntValue>();
      llvm::APInt ap_value = sem_ir.ints().Get(int_val.int_id);
      // Sign-extend or truncate to 64 bits for our default integer type.
      auto* value = llvm::ConstantInt::get(
          llvm::Type::getInt64Ty(context.llvm_context()),
          ap_value.sextOrTrunc(64));
      context.SetLocal(inst_id, value);
      break;
    }

    case SemIR::InstKind::FloatValue: {
      auto float_val = inst.As<SemIR::FloatValue>();
      llvm::APFloat ap_value = sem_ir.floats().Get(float_val.float_id);
      llvm::Type* float_type = context.GetType(float_val.type_id);
      auto* value = llvm::ConstantFP::get(float_type, ap_value);
      context.SetLocal(inst_id, value);
      break;
    }

    case SemIR::InstKind::StringLiteral: {
      auto str_lit = inst.As<SemIR::StringLiteral>();
      llvm::StringRef str_value =
          sem_ir.string_literal_values().Get(str_lit.string_id);
      auto* value = context.builder().CreateGlobalString(str_value);
      context.SetLocal(inst_id, value);
      break;
    }

    // -----------------------------------------------------------------------
    // Variables and bindings
    // -----------------------------------------------------------------------
    case SemIR::InstKind::VarStorage: {
      auto var = inst.As<SemIR::VarStorage>();
      llvm::Type* var_type = context.GetType(var.type_id);
      // Alloca at the current insert point.
      auto* alloca = context.builder().CreateAlloca(var_type, nullptr);
      context.SetLocal(inst_id, alloca);
      break;
    }

    case SemIR::InstKind::Assign: {
      auto assign = inst.As<SemIR::Assign>();
      auto* lhs = context.GetLocal(assign.lhs_id);
      auto* rhs = context.GetLocal(assign.rhs_id);
      context.builder().CreateStore(rhs, lhs);
      break;
    }

    case SemIR::InstKind::NameRef: {
      auto name_ref = inst.As<SemIR::NameRef>();
      auto* value = context.TryGetLocal(name_ref.value_id);
      if (value) {
        context.SetLocal(inst_id, value);
      }
      break;
    }

    case SemIR::InstKind::ValueBinding: {
      auto binding = inst.As<SemIR::ValueBinding>();
      auto* value = context.TryGetLocal(binding.value_id);
      if (value) {
        context.SetLocal(inst_id, value);
      }
      break;
    }

    case SemIR::InstKind::Converted: {
      auto converted = inst.As<SemIR::Converted>();
      auto* value = context.TryGetLocal(converted.result_id);
      if (value) {
        context.SetLocal(inst_id, value);
      }
      break;
    }

    case SemIR::InstKind::SpliceBlock: {
      auto splice = inst.As<SemIR::SpliceBlock>();
      // Lower all instructions in the spliced block inline.
      auto block_insts = sem_ir.inst_blocks().Get(splice.block_id);
      for (auto block_inst_id : block_insts) {
        LowerInst(context, block_inst_id);
      }
      // The result of the splice is the result of the last expression.
      auto* value = context.TryGetLocal(splice.result_id);
      if (value) {
        context.SetLocal(inst_id, value);
      }
      break;
    }

    // -----------------------------------------------------------------------
    // Function parameters
    // -----------------------------------------------------------------------
    case SemIR::InstKind::ValueParam: {
      // ValueParam is mapped to the LLVM function argument during function
      // body lowering. If it's already set, nothing to do here.
      break;
    }

    // -----------------------------------------------------------------------
    // Integer arithmetic
    // -----------------------------------------------------------------------
    case SemIR::InstKind::IntAdd: {
      auto op = inst.As<SemIR::IntAdd>();
      auto* lhs = context.GetLocal(op.lhs_id);
      auto* rhs = context.GetLocal(op.rhs_id);
      context.SetLocal(inst_id, context.builder().CreateAdd(lhs, rhs));
      break;
    }

    case SemIR::InstKind::IntSub: {
      auto op = inst.As<SemIR::IntSub>();
      auto* lhs = context.GetLocal(op.lhs_id);
      auto* rhs = context.GetLocal(op.rhs_id);
      context.SetLocal(inst_id, context.builder().CreateSub(lhs, rhs));
      break;
    }

    case SemIR::InstKind::IntMul: {
      auto op = inst.As<SemIR::IntMul>();
      auto* lhs = context.GetLocal(op.lhs_id);
      auto* rhs = context.GetLocal(op.rhs_id);
      context.SetLocal(inst_id, context.builder().CreateMul(lhs, rhs));
      break;
    }

    case SemIR::InstKind::IntDiv: {
      auto op = inst.As<SemIR::IntDiv>();
      auto* lhs = context.GetLocal(op.lhs_id);
      auto* rhs = context.GetLocal(op.rhs_id);
      context.SetLocal(inst_id, context.builder().CreateSDiv(lhs, rhs));
      break;
    }

    case SemIR::InstKind::IntMod: {
      auto op = inst.As<SemIR::IntMod>();
      auto* lhs = context.GetLocal(op.lhs_id);
      auto* rhs = context.GetLocal(op.rhs_id);
      context.SetLocal(inst_id, context.builder().CreateSRem(lhs, rhs));
      break;
    }

    case SemIR::InstKind::IntNegate: {
      auto op = inst.As<SemIR::IntNegate>();
      auto* operand = context.GetLocal(op.operand_id);
      context.SetLocal(inst_id, context.builder().CreateNeg(operand));
      break;
    }

    // -----------------------------------------------------------------------
    // Integer comparisons
    // -----------------------------------------------------------------------
    case SemIR::InstKind::IntEq: {
      auto op = inst.As<SemIR::IntEq>();
      auto* lhs = context.GetLocal(op.lhs_id);
      auto* rhs = context.GetLocal(op.rhs_id);
      context.SetLocal(inst_id, context.builder().CreateICmpEQ(lhs, rhs));
      break;
    }

    case SemIR::InstKind::IntNeq: {
      auto op = inst.As<SemIR::IntNeq>();
      auto* lhs = context.GetLocal(op.lhs_id);
      auto* rhs = context.GetLocal(op.rhs_id);
      context.SetLocal(inst_id, context.builder().CreateICmpNE(lhs, rhs));
      break;
    }

    case SemIR::InstKind::IntLess: {
      auto op = inst.As<SemIR::IntLess>();
      auto* lhs = context.GetLocal(op.lhs_id);
      auto* rhs = context.GetLocal(op.rhs_id);
      context.SetLocal(inst_id, context.builder().CreateICmpSLT(lhs, rhs));
      break;
    }

    case SemIR::InstKind::IntGreater: {
      auto op = inst.As<SemIR::IntGreater>();
      auto* lhs = context.GetLocal(op.lhs_id);
      auto* rhs = context.GetLocal(op.rhs_id);
      context.SetLocal(inst_id, context.builder().CreateICmpSGT(lhs, rhs));
      break;
    }

    case SemIR::InstKind::IntLessEq: {
      auto op = inst.As<SemIR::IntLessEq>();
      auto* lhs = context.GetLocal(op.lhs_id);
      auto* rhs = context.GetLocal(op.rhs_id);
      context.SetLocal(inst_id, context.builder().CreateICmpSLE(lhs, rhs));
      break;
    }

    case SemIR::InstKind::IntGreaterEq: {
      auto op = inst.As<SemIR::IntGreaterEq>();
      auto* lhs = context.GetLocal(op.lhs_id);
      auto* rhs = context.GetLocal(op.rhs_id);
      context.SetLocal(inst_id, context.builder().CreateICmpSGE(lhs, rhs));
      break;
    }

    // -----------------------------------------------------------------------
    // Boolean operations
    // -----------------------------------------------------------------------
    case SemIR::InstKind::BoolNot: {
      auto op = inst.As<SemIR::BoolNot>();
      auto* operand = context.GetLocal(op.operand_id);
      context.SetLocal(inst_id, context.builder().CreateNot(operand));
      break;
    }

    case SemIR::InstKind::BoolAnd: {
      auto op = inst.As<SemIR::BoolAnd>();
      auto* lhs = context.GetLocal(op.lhs_id);
      auto* rhs = context.GetLocal(op.rhs_id);
      context.SetLocal(inst_id, context.builder().CreateAnd(lhs, rhs));
      break;
    }

    case SemIR::InstKind::BoolOr: {
      auto op = inst.As<SemIR::BoolOr>();
      auto* lhs = context.GetLocal(op.lhs_id);
      auto* rhs = context.GetLocal(op.rhs_id);
      context.SetLocal(inst_id, context.builder().CreateOr(lhs, rhs));
      break;
    }

    // -----------------------------------------------------------------------
    // Control flow
    // -----------------------------------------------------------------------
    case SemIR::InstKind::Return: {
      context.builder().CreateRetVoid();
      break;
    }

    case SemIR::InstKind::ReturnExpr: {
      auto ret = inst.As<SemIR::ReturnExpr>();
      auto* value = context.GetLocal(ret.expr_id);
      context.builder().CreateRet(value);
      break;
    }

    case SemIR::InstKind::Branch: {
      auto branch = inst.As<SemIR::Branch>();
      auto* target = context.GetBlock(branch.target_id);
      context.builder().CreateBr(target);
      break;
    }

    case SemIR::InstKind::BranchIf: {
      auto branch_if = inst.As<SemIR::BranchIf>();
      auto* cond = context.GetLocal(branch_if.cond_id);
      auto* target = context.GetBlock(branch_if.target_id);
      // The fallthrough block is the next block in sequence; it will be
      // created by the Branch instruction that follows.
      // For now, create a placeholder that the next block will fill.
      auto* current_fn = context.builder().GetInsertBlock()->getParent();
      auto* fallthrough =
          llvm::BasicBlock::Create(context.llvm_context(), "", current_fn);
      context.builder().CreateCondBr(cond, target, fallthrough);
      context.builder().SetInsertPoint(fallthrough);
      break;
    }

    case SemIR::InstKind::BranchWithArg: {
      auto branch = inst.As<SemIR::BranchWithArg>();
      auto* arg = context.GetLocal(branch.arg_id);
      auto* target = context.GetBlock(branch.target_id);
      context.SetBlockArg(branch.target_id, arg);
      context.builder().CreateBr(target);
      break;
    }

    case SemIR::InstKind::BlockArg: {
      auto block_arg = inst.As<SemIR::BlockArg>();
      auto* value = context.GetBlockArg(block_arg.block_id);
      context.SetLocal(inst_id, value);
      break;
    }

    // -----------------------------------------------------------------------
    // Function calls
    // -----------------------------------------------------------------------
    case SemIR::InstKind::Call: {
      auto call = inst.As<SemIR::Call>();
      // Resolve the callee to a function.
      auto callee = SemIR::GetCalleeAsFunction(sem_ir, call.callee_id);
      auto* callee_fn = context.GetFunction(callee.function_id);

      // Gather arguments.
      llvm::SmallVector<llvm::Value*> args;
      if (call.args_id.has_value() &&
          call.args_id != SemIR::InstBlockId::Empty) {
        auto arg_ids = sem_ir.inst_blocks().Get(call.args_id);
        for (auto arg_id : arg_ids) {
          auto* arg_val = context.TryGetLocal(arg_id);
          if (arg_val) {
            args.push_back(arg_val);
          }
        }
      }

      auto* result = context.builder().CreateCall(callee_fn, args);
      // Only set local if the call produces a non-void value.
      if (!callee_fn->getReturnType()->isVoidTy()) {
        context.SetLocal(inst_id, result);
      }
      break;
    }

    // -----------------------------------------------------------------------
    // Struct operations
    // -----------------------------------------------------------------------
    case SemIR::InstKind::StructInit: {
      auto struct_init = inst.As<SemIR::StructInit>();
      llvm::Type* struct_type = context.GetType(struct_init.type_id);

      // Start with an undef value and insert elements.
      llvm::Value* agg = llvm::UndefValue::get(struct_type);
      if (struct_init.args_id.has_value() &&
          struct_init.args_id != SemIR::InstBlockId::Empty) {
        auto field_ids = sem_ir.inst_blocks().Get(struct_init.args_id);
        for (auto [i, field_id] : llvm::enumerate(field_ids)) {
          auto* field_val = context.TryGetLocal(field_id);
          if (field_val) {
            agg = context.builder().CreateInsertValue(agg, field_val, {static_cast<unsigned>(i)});
          }
        }
      }
      context.SetLocal(inst_id, agg);
      break;
    }

    case SemIR::InstKind::FieldAccess: {
      auto access = inst.As<SemIR::FieldAccess>();
      auto* base = context.GetLocal(access.base_id);

      // If the base is a pointer (alloca), use GEP + load.
      if (base->getType()->isPointerTy()) {
        auto base_inst = sem_ir.insts().Get(access.base_id);
        llvm::Type* struct_type = context.GetType(base_inst.type_id());
        auto* gep = context.builder().CreateStructGEP(
            struct_type, base, access.index.index);
        llvm::Type* field_type = context.GetType(access.type_id);
        auto* loaded = context.builder().CreateLoad(field_type, gep);
        context.SetLocal(inst_id, loaded);
      } else {
        // Direct aggregate extraction.
        auto* extracted = context.builder().CreateExtractValue(
            base, {static_cast<unsigned>(access.index.index)});
        context.SetLocal(inst_id, extracted);
      }
      break;
    }

    // -----------------------------------------------------------------------
    // Optional operations
    // -----------------------------------------------------------------------
    case SemIR::InstKind::OptionalNone: {
      auto opt_none = inst.As<SemIR::OptionalNone>();
      llvm::Type* opt_type = context.GetType(opt_none.type_id);
      // Create {i1 false, zeroinitializer}.
      auto* value = llvm::Constant::getNullValue(opt_type);
      context.SetLocal(inst_id, value);
      break;
    }

    case SemIR::InstKind::OptionalSome: {
      auto opt_some = inst.As<SemIR::OptionalSome>();
      llvm::Type* opt_type = context.GetType(opt_some.type_id);
      auto* inner = context.GetLocal(opt_some.value_id);
      // Build {i1 true, inner_value}.
      llvm::Value* agg = llvm::UndefValue::get(opt_type);
      agg = context.builder().CreateInsertValue(
          agg, llvm::ConstantInt::getTrue(context.llvm_context()), {0});
      agg = context.builder().CreateInsertValue(agg, inner, {1});
      context.SetLocal(inst_id, agg);
      break;
    }

    // -----------------------------------------------------------------------
    // Float arithmetic
    // -----------------------------------------------------------------------
    case SemIR::InstKind::FloatAdd: {
      auto op = inst.As<SemIR::FloatAdd>();
      auto* lhs = context.GetLocal(op.lhs_id);
      auto* rhs = context.GetLocal(op.rhs_id);
      context.SetLocal(inst_id, context.builder().CreateFAdd(lhs, rhs));
      break;
    }

    case SemIR::InstKind::FloatSub: {
      auto op = inst.As<SemIR::FloatSub>();
      auto* lhs = context.GetLocal(op.lhs_id);
      auto* rhs = context.GetLocal(op.rhs_id);
      context.SetLocal(inst_id, context.builder().CreateFSub(lhs, rhs));
      break;
    }

    case SemIR::InstKind::FloatMul: {
      auto op = inst.As<SemIR::FloatMul>();
      auto* lhs = context.GetLocal(op.lhs_id);
      auto* rhs = context.GetLocal(op.rhs_id);
      context.SetLocal(inst_id, context.builder().CreateFMul(lhs, rhs));
      break;
    }

    case SemIR::InstKind::FloatDiv: {
      auto op = inst.As<SemIR::FloatDiv>();
      auto* lhs = context.GetLocal(op.lhs_id);
      auto* rhs = context.GetLocal(op.rhs_id);
      context.SetLocal(inst_id, context.builder().CreateFDiv(lhs, rhs));
      break;
    }

    case SemIR::InstKind::FloatNegate: {
      auto op = inst.As<SemIR::FloatNegate>();
      auto* operand = context.GetLocal(op.operand_id);
      context.SetLocal(inst_id, context.builder().CreateFNeg(operand));
      break;
    }

    // -----------------------------------------------------------------------
    // Float comparisons
    // -----------------------------------------------------------------------
    case SemIR::InstKind::FloatEq: {
      auto op = inst.As<SemIR::FloatEq>();
      auto* lhs = context.GetLocal(op.lhs_id);
      auto* rhs = context.GetLocal(op.rhs_id);
      context.SetLocal(inst_id, context.builder().CreateFCmpOEQ(lhs, rhs));
      break;
    }

    case SemIR::InstKind::FloatNeq: {
      auto op = inst.As<SemIR::FloatNeq>();
      auto* lhs = context.GetLocal(op.lhs_id);
      auto* rhs = context.GetLocal(op.rhs_id);
      context.SetLocal(inst_id, context.builder().CreateFCmpONE(lhs, rhs));
      break;
    }

    case SemIR::InstKind::FloatLess: {
      auto op = inst.As<SemIR::FloatLess>();
      auto* lhs = context.GetLocal(op.lhs_id);
      auto* rhs = context.GetLocal(op.rhs_id);
      context.SetLocal(inst_id, context.builder().CreateFCmpOLT(lhs, rhs));
      break;
    }

    case SemIR::InstKind::FloatGreater: {
      auto op = inst.As<SemIR::FloatGreater>();
      auto* lhs = context.GetLocal(op.lhs_id);
      auto* rhs = context.GetLocal(op.rhs_id);
      context.SetLocal(inst_id, context.builder().CreateFCmpOGT(lhs, rhs));
      break;
    }

    case SemIR::InstKind::FloatLessEq: {
      auto op = inst.As<SemIR::FloatLessEq>();
      auto* lhs = context.GetLocal(op.lhs_id);
      auto* rhs = context.GetLocal(op.rhs_id);
      context.SetLocal(inst_id, context.builder().CreateFCmpOLE(lhs, rhs));
      break;
    }

    case SemIR::InstKind::FloatGreaterEq: {
      auto op = inst.As<SemIR::FloatGreaterEq>();
      auto* lhs = context.GetLocal(op.lhs_id);
      auto* rhs = context.GetLocal(op.rhs_id);
      context.SetLocal(inst_id, context.builder().CreateFCmpOGE(lhs, rhs));
      break;
    }

    // -----------------------------------------------------------------------
    // Type conversions
    // -----------------------------------------------------------------------
    case SemIR::InstKind::IntToFloat: {
      auto op = inst.As<SemIR::IntToFloat>();
      auto* operand = context.GetLocal(op.operand_id);
      auto* target_type = context.GetType(op.type_id);
      context.SetLocal(inst_id,
                        context.builder().CreateSIToFP(operand, target_type));
      break;
    }

    case SemIR::InstKind::FloatToInt: {
      auto op = inst.As<SemIR::FloatToInt>();
      auto* operand = context.GetLocal(op.operand_id);
      auto* target_type = context.GetType(op.type_id);
      context.SetLocal(inst_id,
                        context.builder().CreateFPToSI(operand, target_type));
      break;
    }

    // -----------------------------------------------------------------------
    // String operations
    // -----------------------------------------------------------------------
    case SemIR::InstKind::StringConcat: {
      // String concatenation is a runtime operation - for now, just forward lhs.
      auto op = inst.As<SemIR::StringConcat>();
      auto* lhs = context.GetLocal(op.lhs_id);
      context.SetLocal(inst_id, lhs);
      break;
    }

    // -----------------------------------------------------------------------
    // Enum operations
    // -----------------------------------------------------------------------
    case SemIR::InstKind::EnumType:
    case SemIR::InstKind::EnumCase:
    case SemIR::InstKind::EnumCaseWithPayload:
      // Type-level instructions, not lowered directly.
      break;

    case SemIR::InstKind::EnumInit: {
      auto op = inst.As<SemIR::EnumInit>();
      auto* enum_type = context.GetType(op.type_id);
      // Create a tagged union: store discriminant in first field.
      auto* case_val = context.TryGetLocal(op.case_id);
      llvm::Value* agg = llvm::UndefValue::get(enum_type);
      if (case_val) {
        agg = context.builder().CreateInsertValue(agg, case_val, {0});
      }
      context.SetLocal(inst_id, agg);
      break;
    }

    case SemIR::InstKind::EnumDiscriminant: {
      auto op = inst.As<SemIR::EnumDiscriminant>();
      auto* enum_val = context.GetLocal(op.enum_id);
      auto* tag = context.builder().CreateExtractValue(enum_val, {0});
      context.SetLocal(inst_id, tag);
      break;
    }

    case SemIR::InstKind::EnumPayload: {
      auto op = inst.As<SemIR::EnumPayload>();
      auto* enum_val = context.GetLocal(op.enum_id);
      // Extract payload bytes (index 1 of the tagged union).
      auto* payload = context.builder().CreateExtractValue(enum_val, {1});
      context.SetLocal(inst_id, payload);
      break;
    }

    // -----------------------------------------------------------------------
    // Tuple operations
    // -----------------------------------------------------------------------
    case SemIR::InstKind::TupleType:
      // Type instruction, not lowered.
      break;

    case SemIR::InstKind::TupleInit: {
      auto op = inst.As<SemIR::TupleInit>();
      auto* tuple_type = context.GetType(op.type_id);
      llvm::Value* agg = llvm::UndefValue::get(tuple_type);
      if (op.elements_id.has_value() &&
          op.elements_id != SemIR::InstBlockId::Empty) {
        auto elem_ids = sem_ir.inst_blocks().Get(op.elements_id);
        for (auto [i, elem_id] : llvm::enumerate(elem_ids)) {
          auto* elem_val = context.TryGetLocal(elem_id);
          if (elem_val) {
            agg = context.builder().CreateInsertValue(
                agg, elem_val, {static_cast<unsigned>(i)});
          }
        }
      }
      context.SetLocal(inst_id, agg);
      break;
    }

    case SemIR::InstKind::TupleAccess: {
      auto op = inst.As<SemIR::TupleAccess>();
      auto* tuple = context.GetLocal(op.tuple_id);
      auto* extracted = context.builder().CreateExtractValue(
          tuple, {static_cast<unsigned>(op.index.index)});
      context.SetLocal(inst_id, extracted);
      break;
    }

    // -----------------------------------------------------------------------
    // Struct field (not lowered, metadata only)
    // -----------------------------------------------------------------------
    case SemIR::InstKind::StructField:
      break;

    // -----------------------------------------------------------------------
    // Closure operations (stubs)
    // -----------------------------------------------------------------------
    case SemIR::InstKind::ClosureExpr:
    case SemIR::InstKind::CaptureRef:
      // Closures will be properly lowered via TinySIL.
      break;

    // -----------------------------------------------------------------------
    // Pattern matching (stubs)
    // -----------------------------------------------------------------------
    case SemIR::InstKind::SwitchInst:
    case SemIR::InstKind::CasePattern:
      // Pattern matching will be properly lowered via TinySIL.
      break;
  }
}

}  // namespace TinySwift::Lower
