// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "toolchain/lower/lower.h"

#include <memory>

#include "common/vlog.h"
#include "llvm/BinaryFormat/Dwarf.h"
#include "llvm/IR/DIBuilder.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Verifier.h"
#include "toolchain/lower/context.h"
#include "toolchain/sem_ir/function.h"
#include "toolchain/sem_ir/typed_insts.h"
#include "toolchain/tiny_sil/function.h"
#include "toolchain/tiny_sil/instruction.h"
#include "toolchain/tiny_sil/printer.h"

namespace TinySwift::Lower {

// Resolves the function name from a SemIR Function.
static auto GetFunctionName(const SemIR::File& sem_ir,
                            const SemIR::Function& function)
    -> llvm::StringRef {
  auto identifier_id = function.name_id.AsIdentifierId();
  if (identifier_id.has_value()) {
    return sem_ir.identifiers().Get(identifier_id);
  }
  return "";
}

// Builds the LLVM FunctionType for a SemIR Function.
static auto BuildFunctionType(Context& context,
                              const SemIR::Function& function)
    -> llvm::FunctionType* {
  auto& sem_ir = context.sem_ir();

  // Determine return type.
  llvm::Type* ret_type;
  if (function.return_type_inst_id.has_value()) {
    auto ret_type_id =
        sem_ir.types().GetTypeIdForTypeInstId(function.return_type_inst_id);
    ret_type = context.GetType(ret_type_id);
    // Void types (e.g. TypeType) should produce void returns.
    if (ret_type->isVoidTy()) {
      ret_type = llvm::Type::getVoidTy(context.llvm_context());
    }
  } else {
    ret_type = llvm::Type::getVoidTy(context.llvm_context());
  }

  // Determine parameter types from call_params_id.
  llvm::SmallVector<llvm::Type*> param_types;
  if (function.call_params_id.has_value() &&
      function.call_params_id != SemIR::InstBlockId::Empty) {
    auto param_ids = sem_ir.inst_blocks().Get(function.call_params_id);
    for (auto param_id : param_ids) {
      auto param_inst = sem_ir.insts().Get(param_id);
      if (auto value_param = param_inst.TryAs<SemIR::ValueParam>()) {
        llvm::Type* param_type = context.GetType(value_param->type_id);
        if (!param_type->isVoidTy()) {
          param_types.push_back(param_type);
        }
      } else if (param_inst.Is<SemIR::InoutParam>()) {
        // M40: inout params are passed as opaque pointers.
        param_types.push_back(llvm::PointerType::get(context.llvm_context(), 0));
      }
    }
  }

  return llvm::FunctionType::get(ret_type, param_types, /*isVarArg=*/false);
}

// Forward-declares all SemIR functions as LLVM functions.
static auto DeclareFunctions(Context& context) -> void {
  auto& sem_ir = context.sem_ir();

  for (auto [func_id, function] : sem_ir.functions().enumerate()) {
    llvm::StringRef name = GetFunctionName(sem_ir, function);
    if (name.empty()) {
      continue;
    }

    // M75: For @extern("C") functions, use the extern symbol name.
    std::string effective_name;
    if (function.is_extern_c && !function.extern_name.empty()) {
      effective_name = function.extern_name;
    }
    // M76: For @cdecl functions, use the cdecl export name.
    else if (function.is_cdecl && !function.cdecl_name.empty()) {
      effective_name = function.cdecl_name;
    } else {
      effective_name = name.str();
    }

    auto* fn_type = BuildFunctionType(context, function);
    auto* llvm_fn = llvm::Function::Create(
        fn_type, llvm::Function::ExternalLinkage, effective_name,
        &context.module());

    // M76: Set C calling convention for @cdecl functions.
    if (function.is_cdecl) {
      llvm_fn->setCallingConv(llvm::CallingConv::C);
      llvm_fn->setVisibility(llvm::GlobalValue::DefaultVisibility);
    }

    // M119: Create DISubprogram for debug info.
    if (context.debug_enabled()) {
      unsigned line = 0, col = 0;
      // Try to get source location from the first body block instruction.
      if (!function.body_block_ids.empty()) {
        auto first_block = sem_ir.inst_blocks().Get(function.body_block_ids[0]);
        if (!first_block.empty()) {
          auto loc_id = sem_ir.insts().GetCanonicalLocId(first_block[0]);
          context.ResolveLocToLineCol(loc_id, line, col);
        }
      }
      auto* sp = context.di_builder()->createFunction(
          context.di_file(), effective_name, effective_name,
          context.di_file(), line,
          context.CreateFunctionDIType(), line,
          llvm::DINode::FlagPrototyped,
          llvm::DISubprogram::SPFlagDefinition);
      llvm_fn->setSubprogram(sp);
    }

    context.SetFunction(func_id, llvm_fn);
  }
}

// Lowers a single function body.
static auto LowerFunctionBody(Context& context, SemIR::FunctionId func_id,
                              const SemIR::Function& function) -> void {
  auto& sem_ir = context.sem_ir();
  auto* llvm_fn = context.GetFunction(func_id);

  if (function.body_block_ids.empty()) {
    return;
  }

  // M119: Set debug scope to the function's DISubprogram.
  if (context.debug_enabled() && llvm_fn->getSubprogram()) {
    context.SetCurrentScope(llvm_fn->getSubprogram());
  }

  // Create basic blocks for all body blocks upfront.
  for (auto block_id : function.body_block_ids) {
    context.GetOrCreateBlock(block_id, llvm_fn);
  }

  // Set the insert point to the entry block.
  auto* entry_block = context.GetBlock(function.body_block_ids[0]);
  context.builder().SetInsertPoint(entry_block);

  // Map ValueParam instructions to LLVM function arguments.
  if (function.call_params_id.has_value() &&
      function.call_params_id != SemIR::InstBlockId::Empty) {
    auto param_ids = sem_ir.inst_blocks().Get(function.call_params_id);
    unsigned arg_index = 0;
    for (auto param_id : param_ids) {
      auto param_inst = sem_ir.insts().Get(param_id);
      if (auto value_param = param_inst.TryAs<SemIR::ValueParam>()) {
        if (arg_index < llvm_fn->arg_size()) {
          context.SetLocal(param_id, llvm_fn->getArg(arg_index));

          // M120: Emit DIParameterVariable for function parameters.
          if (context.debug_enabled() && llvm_fn->getSubprogram()) {
            unsigned line = 0, col = 0;
            auto loc_id = sem_ir.insts().GetCanonicalLocId(param_id);
            context.ResolveLocToLineCol(loc_id, line, col);
            auto* di_type = context.GetOrCreateDIType(value_param->type_id);
            if (di_type) {
              auto* di_param = context.di_builder()->createParameterVariable(
                  llvm_fn->getSubprogram(),
                  llvm_fn->getArg(arg_index)->getName(), arg_index + 1,
                  context.di_file(), line, di_type);
              context.di_builder()->insertDeclare(
                  llvm_fn->getArg(arg_index), di_param,
                  context.di_builder()->createExpression(),
                  llvm::DILocation::get(context.llvm_context(), line, col,
                                        llvm_fn->getSubprogram()),
                  entry_block);
            }
          }

          ++arg_index;
        }
      } else if (param_inst.Is<SemIR::InoutParam>()) {
        // M40: inout param is a pointer argument — map directly, no load.
        if (arg_index < llvm_fn->arg_size()) {
          context.SetLocal(param_id, llvm_fn->getArg(arg_index));
          ++arg_index;
        }
      }
    }
  }

  // Lower instructions in each body block.
  for (auto block_id : function.body_block_ids) {
    auto* bb = context.GetBlock(block_id);
    context.builder().SetInsertPoint(bb);

    auto block_insts = sem_ir.inst_blocks().Get(block_id);
    for (auto inst_id : block_insts) {
      // M119: Set debug location before each instruction.
      if (context.debug_enabled()) {
        auto loc_id = sem_ir.insts().GetCanonicalLocId(inst_id);
        context.SetDebugLoc(loc_id);
      }
      LowerInst(context, inst_id);
    }

    // If the block has no terminator, add a default one.
    if (bb->getTerminator() == nullptr) {
      if (llvm_fn->getReturnType()->isVoidTy()) {
        context.builder().CreateRetVoid();
      } else {
        context.builder().CreateUnreachable();
      }
    }
  }
}

// Lowers all function bodies.
static auto LowerFunctionBodies(Context& context) -> void {
  auto& sem_ir = context.sem_ir();

  for (auto [func_id, function] : sem_ir.functions().enumerate()) {
    if (!function.body_block_ids.empty()) {
      LowerFunctionBody(context, func_id, function);
    }
  }
}

// Lowers top-level instructions into a global init function if needed.
static auto LowerTopLevelInsts(Context& context) -> void {
  auto& sem_ir = context.sem_ir();
  auto top_block_id = sem_ir.top_inst_block_id();
  if (!top_block_id.has_value()) {
    return;
  }

  auto top_insts = sem_ir.inst_blocks().Get(top_block_id);
  if (top_insts.empty()) {
    return;
  }

  // Check if any top-level instructions need runtime lowering.
  bool has_runtime_insts = false;
  for (auto inst_id : top_insts) {
    auto inst = sem_ir.insts().Get(inst_id);
    auto kind = inst.kind();
    // Skip purely compile-time instructions.
    if (kind == SemIR::InstKind::Namespace ||
        kind == SemIR::InstKind::ImportDecl ||
        kind == SemIR::InstKind::FunctionDecl ||
        kind == SemIR::InstKind::NameBindingDecl) {
      continue;
    }
    // Type instructions are not runtime values.
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
        kind == SemIR::InstKind::StructField ||
        kind == SemIR::InstKind::BoundMethod ||
        kind == SemIR::InstKind::GeneratorType ||
        kind == SemIR::InstKind::AsyncFuncType ||
        kind == SemIR::InstKind::Yield) {
      continue;
    }
    // Non-lowered patterns.
    if (kind == SemIR::InstKind::ValueBindingPattern ||
        kind == SemIR::InstKind::ValueParamPattern) {
      continue;
    }
    has_runtime_insts = true;
    break;
  }

  if (!has_runtime_insts) {
    return;
  }

  // Create the global init function.
  auto* void_type = llvm::Type::getVoidTy(context.llvm_context());
  auto* init_fn_type = llvm::FunctionType::get(void_type, /*isVarArg=*/false);
  auto* init_fn = llvm::Function::Create(
      init_fn_type, llvm::Function::ExternalLinkage, "__tinyswift_init",
      &context.module());
  auto* entry =
      llvm::BasicBlock::Create(context.llvm_context(), "entry", init_fn);
  context.builder().SetInsertPoint(entry);

  for (auto inst_id : top_insts) {
    LowerInst(context, inst_id);
  }

  // Terminate the init function.
  if (entry->getTerminator() == nullptr) {
    context.builder().CreateRetVoid();
  }
}

auto LowerToLLVM(llvm::LLVMContext& llvm_context,
                 llvm::StringRef module_name,
                 const SemIR::File& sem_ir,
                 const LowerToLLVMOptions& options,
                 const Lex::TokenizedBuffer* tokens)
    -> std::unique_ptr<llvm::Module> {
  auto module = std::make_unique<llvm::Module>(module_name, llvm_context);

  TINYSWIFT_VLOG_TO(options.vlog_stream, "*** Lowering: {0} ***\n",
                 sem_ir.filename());

  Context context(*module, llvm_context, sem_ir, tokens,
                  options.want_debug_info);

  // Step 1: Forward-declare all functions.
  DeclareFunctions(context);

  // Step 2: Lower all function bodies.
  LowerFunctionBodies(context);

  // Step 3: Lower top-level instructions (global init).
  LowerTopLevelInsts(context);

  // M119: Finalize debug info before verification.
  if (context.debug_enabled()) {
    context.di_builder()->finalize();
  }

  if (options.vlog_stream) {
    TINYSWIFT_VLOG_TO(options.vlog_stream, "*** llvm::Module ***\n");
    module->print(*options.vlog_stream, /*AAW=*/nullptr,
                  /*ShouldPreserveUseListOrder=*/false,
                  /*IsForDebug=*/true);
  }

  if (options.llvm_verifier_stream) {
    TINYSWIFT_CHECK(!llvm::verifyModule(*module, options.llvm_verifier_stream));
  }

  return module;
}

// =============================================================================
// TinySIL → LLVM IR lowering
// =============================================================================

namespace {

// Gets an LLVM type for a SIL type index using the SemIR type store.
auto GetLLVMTypeForSIL(Context& context, const TinySIL::SILType& sil_type)
    -> llvm::Type* {
  if (!sil_type.is_valid()) {
    return llvm::Type::getVoidTy(context.llvm_context());
  }
  auto type_id = SemIR::TypeId(sil_type.type_index);
  auto* ty = context.GetType(type_id);
  if (sil_type.is_address) {
    return llvm::PointerType::get(ty->getContext(), 0);
  }
  return ty;
}

// Builds LLVM FunctionType from a SIL function.
auto BuildSILFunctionType(Context& context,
                          const TinySIL::SILFunction& sil_fn)
    -> llvm::FunctionType* {
  llvm::Type* ret_type;
  if (sil_fn.type.is_void_return) {
    ret_type = llvm::Type::getVoidTy(context.llvm_context());
  } else {
    ret_type = GetLLVMTypeForSIL(context, sil_fn.type.return_type);
    if (ret_type->isVoidTy()) {
      ret_type = llvm::Type::getVoidTy(context.llvm_context());
    }
  }

  llvm::SmallVector<llvm::Type*> param_types;
  for (const auto& param_type : sil_fn.type.param_types) {
    auto* ty = GetLLVMTypeForSIL(context, param_type);
    if (!ty->isVoidTy()) {
      param_types.push_back(ty);
    }
  }

  return llvm::FunctionType::get(ret_type, param_types, /*isVarArg=*/false);
}

// Lowers a single SIL instruction to LLVM IR.
auto LowerSILInst(
    Context& context,
    const TinySIL::SILInstruction& inst,
    llvm::DenseMap<int32_t, llvm::Value*>& sil_values,
    llvm::DenseMap<int32_t, llvm::BasicBlock*>& sil_blocks,
    llvm::Function* /*llvm_fn*/,
    llvm::DenseMap<int32_t, llvm::SmallVector<llvm::Value*>>&
        closure_captures) -> void {
  auto& builder = context.builder();

  auto getSILValue = [&](const TinySIL::SILValue& val) -> llvm::Value* {
    if (!val.is_valid()) return nullptr;
    auto it = sil_values.find(val.id);
    return (it != sil_values.end()) ? it->second : nullptr;
  };

  auto setSILValue = [&](const TinySIL::SILValue& val, llvm::Value* llvm_val) {
    if (val.is_valid() && llvm_val) {
      sil_values[val.id] = llvm_val;
    }
  };

  auto getBlock = [&](int32_t block_id) -> llvm::BasicBlock* {
    auto it = sil_blocks.find(block_id);
    return (it != sil_blocks.end()) ? it->second : nullptr;
  };

  switch (inst.kind) {
    case TinySIL::SILInstKind::IntegerLiteral: {
      auto* ty = GetLLVMTypeForSIL(context, inst.result.type);
      if (ty->isVoidTy()) ty = builder.getInt64Ty();
      if (!ty->isIntegerTy()) ty = builder.getInt64Ty();
      // i1 (bool) uses unsigned semantics: signed range is {-1,0}, not {0,1}.
      bool is_signed = !ty->isIntegerTy(1);
      auto* val = llvm::ConstantInt::get(ty, inst.literal_value, is_signed);
      setSILValue(inst.result, val);
      break;
    }

    case TinySIL::SILInstKind::FloatLiteral: {
      auto* val = llvm::ConstantFP::get(builder.getDoubleTy(),
                                        inst.float_literal_value);
      setSILValue(inst.result, val);
      break;
    }

    case TinySIL::SILInstKind::StringLiteral: {
      auto* val = builder.CreateGlobalString(inst.string_literal_value);
      setSILValue(inst.result, val);
      break;
    }

    case TinySIL::SILInstKind::AllocStack: {
      auto* alloc_ty = GetLLVMTypeForSIL(context, inst.alloc_type);
      if (alloc_ty->isVoidTy()) alloc_ty = builder.getInt64Ty();
      auto* alloca_val = builder.CreateAlloca(alloc_ty, nullptr);
      setSILValue(inst.result, alloca_val);

      // M120: Emit debug variable info for SIL stack allocations.
      if (context.debug_enabled() && context.current_scope() &&
          inst.loc_id.has_value()) {
        unsigned line = 0, col = 0;
        if (context.ResolveLocToLineCol(inst.loc_id, line, col)) {
          auto sil_type_id = SemIR::TypeId(inst.alloc_type.type_index);
          auto* di_type = context.GetOrCreateDIType(sil_type_id);
          if (di_type) {
            auto* di_var = context.di_builder()->createAutoVariable(
                context.current_scope(), "var", context.di_file(),
                line, di_type);
            context.di_builder()->insertDeclare(
                alloca_val, di_var,
                context.di_builder()->createExpression(),
                llvm::DILocation::get(context.llvm_context(), line, col,
                                      context.current_scope()),
                builder.GetInsertBlock());
          }
        }
      }
      break;
    }

    case TinySIL::SILInstKind::DeallocStack:
      // No-op: stack deallocation is automatic in LLVM.
      break;

    case TinySIL::SILInstKind::Load: {
      auto* addr = getSILValue(inst.operands[0]);
      if (addr) {
        auto* pointee_type = GetLLVMTypeForSIL(context,
                                                inst.result.type);
        if (pointee_type->isVoidTy()) pointee_type = builder.getInt64Ty();
        auto* val = builder.CreateLoad(pointee_type, addr);
        setSILValue(inst.result, val);
      }
      break;
    }

    case TinySIL::SILInstKind::Store: {
      auto* src = getSILValue(inst.operands[0]);
      auto* dst = getSILValue(inst.operands[1]);
      if (src && dst) {
        builder.CreateStore(src, dst);
      }
      break;
    }

    case TinySIL::SILInstKind::FunctionRef: {
      auto* fn = context.module().getFunction(inst.function_name);
      if (!fn) {
        // External function — create declaration.
        auto* fn_ty = llvm::FunctionType::get(builder.getVoidTy(), false);
        fn = llvm::Function::Create(fn_ty, llvm::Function::ExternalLinkage,
                                    inst.function_name, &context.module());
      }
      setSILValue(inst.result, fn);
      break;
    }

    case TinySIL::SILInstKind::PartialApply: {
      // Propagate the underlying FunctionRef value directly.
      auto fn_ref = getSILValue(inst.operands[0]);
      if (fn_ref) setSILValue(inst.result, fn_ref);

      // M53: Store captured values keyed by this result's SIL value ID.
      // The Apply case will prepend these when calling the closure function.
      if (!inst.operand_list.empty() && inst.result.is_valid()) {
        llvm::SmallVector<llvm::Value*> cap_vals;
        for (const auto& cap : inst.operand_list) {
          auto* v = getSILValue(cap);
          if (v) cap_vals.push_back(v);
        }
        if (!cap_vals.empty()) {
          closure_captures[inst.result.id] = std::move(cap_vals);
        }
      }
      break;
    }

    case TinySIL::SILInstKind::Apply: {
      auto* callee = getSILValue(inst.operands[0]);
      if (!callee) break;

      llvm::SmallVector<llvm::Value*> args;

      // M53: Prepend captured values if callee came from a PartialApply.
      if (inst.operands[0].is_valid()) {
        auto cap_it = closure_captures.find(inst.operands[0].id);
        if (cap_it != closure_captures.end()) {
          for (auto* cap_val : cap_it->second) {
            args.push_back(cap_val);
          }
        }
      }

      for (const auto& arg : inst.operand_list) {
        auto* arg_val = getSILValue(arg);
        if (arg_val) args.push_back(arg_val);
      }

      auto* fn = llvm::dyn_cast<llvm::Function>(callee);
      if (fn) {
        // Direct (static) call.
        auto* call = builder.CreateCall(fn, args);
        if (!call->getType()->isVoidTy()) {
          setSILValue(inst.result, call);
        }
      } else {
        // Indirect call through a function pointer (e.g., higher-order param).
        // Build function type from arg types + result type.
        llvm::SmallVector<llvm::Type*> param_types;
        for (auto* a : args) param_types.push_back(a->getType());
        auto* ret_type = GetLLVMTypeForSIL(context, inst.result.type);
        if (!ret_type || ret_type->isVoidTy()) ret_type = builder.getInt64Ty();
        auto* fty =
            llvm::FunctionType::get(ret_type, param_types, /*isVarArg=*/false);
        auto* call = builder.CreateCall(fty, callee, args);
        if (!fty->getReturnType()->isVoidTy()) {
          setSILValue(inst.result, call);
        }
      }
      break;
    }

    case TinySIL::SILInstKind::ReturnInst: {
      auto* ret_val = getSILValue(inst.operands[0]);
      if (ret_val && !ret_val->getType()->isVoidTy()) {
        builder.CreateRet(ret_val);
      } else {
        builder.CreateRetVoid();
      }
      break;
    }

    case TinySIL::SILInstKind::Branch: {
      auto* target = getBlock(inst.target_block);
      if (target) {
        builder.CreateBr(target);
      }
      break;
    }

    case TinySIL::SILInstKind::CondBranch: {
      auto* cond = getSILValue(inst.operands[0]);
      auto* true_bb = getBlock(inst.true_block);
      auto* false_bb = getBlock(inst.false_block);
      if (cond && true_bb && false_bb) {
        // Ensure condition is i1.
        if (!cond->getType()->isIntegerTy(1)) {
          cond = builder.CreateICmpNE(
              cond, llvm::ConstantInt::get(cond->getType(), 0));
        }
        builder.CreateCondBr(cond, true_bb, false_bb);
      }
      break;
    }

    case TinySIL::SILInstKind::Unreachable:
      builder.CreateUnreachable();
      break;

    case TinySIL::SILInstKind::BuiltinInst: {
      auto name = inst.builtin_name;

      // Binary integer ops.
      if (name == "add_Int64") {
        auto* lhs = getSILValue(inst.operands[0]);
        auto* rhs = getSILValue(inst.operands[1]);
        if (lhs && rhs) setSILValue(inst.result, builder.CreateAdd(lhs, rhs));
      } else if (name == "sub_Int64") {
        auto* lhs = getSILValue(inst.operands[0]);
        auto* rhs = getSILValue(inst.operands[1]);
        if (lhs && rhs) setSILValue(inst.result, builder.CreateSub(lhs, rhs));
      } else if (name == "mul_Int64") {
        auto* lhs = getSILValue(inst.operands[0]);
        auto* rhs = getSILValue(inst.operands[1]);
        if (lhs && rhs) setSILValue(inst.result, builder.CreateMul(lhs, rhs));
      } else if (name == "sdiv_Int64") {
        auto* lhs = getSILValue(inst.operands[0]);
        auto* rhs = getSILValue(inst.operands[1]);
        if (lhs && rhs) setSILValue(inst.result, builder.CreateSDiv(lhs, rhs));
      } else if (name == "srem_Int64") {
        auto* lhs = getSILValue(inst.operands[0]);
        auto* rhs = getSILValue(inst.operands[1]);
        if (lhs && rhs) setSILValue(inst.result, builder.CreateSRem(lhs, rhs));
      } else if (name == "neg_Int64") {
        auto* op = getSILValue(inst.operands[0]);
        if (op) setSILValue(inst.result, builder.CreateNeg(op));

      // Integer comparisons.
      } else if (name == "cmp_eq_Int64") {
        auto* lhs = getSILValue(inst.operands[0]);
        auto* rhs = getSILValue(inst.operands[1]);
        if (lhs && rhs) setSILValue(inst.result, builder.CreateICmpEQ(lhs, rhs));
      } else if (name == "cmp_ne_Int64") {
        auto* lhs = getSILValue(inst.operands[0]);
        auto* rhs = getSILValue(inst.operands[1]);
        if (lhs && rhs) setSILValue(inst.result, builder.CreateICmpNE(lhs, rhs));
      } else if (name == "cmp_slt_Int64") {
        auto* lhs = getSILValue(inst.operands[0]);
        auto* rhs = getSILValue(inst.operands[1]);
        if (lhs && rhs) setSILValue(inst.result, builder.CreateICmpSLT(lhs, rhs));
      } else if (name == "cmp_sgt_Int64") {
        auto* lhs = getSILValue(inst.operands[0]);
        auto* rhs = getSILValue(inst.operands[1]);
        if (lhs && rhs) setSILValue(inst.result, builder.CreateICmpSGT(lhs, rhs));
      } else if (name == "cmp_sle_Int64") {
        auto* lhs = getSILValue(inst.operands[0]);
        auto* rhs = getSILValue(inst.operands[1]);
        if (lhs && rhs) setSILValue(inst.result, builder.CreateICmpSLE(lhs, rhs));
      } else if (name == "cmp_sge_Int64") {
        auto* lhs = getSILValue(inst.operands[0]);
        auto* rhs = getSILValue(inst.operands[1]);
        if (lhs && rhs) setSILValue(inst.result, builder.CreateICmpSGE(lhs, rhs));

      // Float arithmetic.
      } else if (name == "fadd_FPIEEE64") {
        auto* lhs = getSILValue(inst.operands[0]);
        auto* rhs = getSILValue(inst.operands[1]);
        if (lhs && rhs) setSILValue(inst.result, builder.CreateFAdd(lhs, rhs));
      } else if (name == "fsub_FPIEEE64") {
        auto* lhs = getSILValue(inst.operands[0]);
        auto* rhs = getSILValue(inst.operands[1]);
        if (lhs && rhs) setSILValue(inst.result, builder.CreateFSub(lhs, rhs));
      } else if (name == "fmul_FPIEEE64") {
        auto* lhs = getSILValue(inst.operands[0]);
        auto* rhs = getSILValue(inst.operands[1]);
        if (lhs && rhs) setSILValue(inst.result, builder.CreateFMul(lhs, rhs));
      } else if (name == "fdiv_FPIEEE64") {
        auto* lhs = getSILValue(inst.operands[0]);
        auto* rhs = getSILValue(inst.operands[1]);
        if (lhs && rhs) setSILValue(inst.result, builder.CreateFDiv(lhs, rhs));
      } else if (name == "fneg_FPIEEE64") {
        auto* op = getSILValue(inst.operands[0]);
        if (op) setSILValue(inst.result, builder.CreateFNeg(op));

      // Float comparisons.
      } else if (name == "fcmp_oeq_FPIEEE64") {
        auto* lhs = getSILValue(inst.operands[0]);
        auto* rhs = getSILValue(inst.operands[1]);
        if (lhs && rhs) setSILValue(inst.result, builder.CreateFCmpOEQ(lhs, rhs));
      } else if (name == "fcmp_one_FPIEEE64") {
        auto* lhs = getSILValue(inst.operands[0]);
        auto* rhs = getSILValue(inst.operands[1]);
        if (lhs && rhs) setSILValue(inst.result, builder.CreateFCmpONE(lhs, rhs));
      } else if (name == "fcmp_olt_FPIEEE64") {
        auto* lhs = getSILValue(inst.operands[0]);
        auto* rhs = getSILValue(inst.operands[1]);
        if (lhs && rhs) setSILValue(inst.result, builder.CreateFCmpOLT(lhs, rhs));
      } else if (name == "fcmp_ogt_FPIEEE64") {
        auto* lhs = getSILValue(inst.operands[0]);
        auto* rhs = getSILValue(inst.operands[1]);
        if (lhs && rhs) setSILValue(inst.result, builder.CreateFCmpOGT(lhs, rhs));
      } else if (name == "fcmp_ole_FPIEEE64") {
        auto* lhs = getSILValue(inst.operands[0]);
        auto* rhs = getSILValue(inst.operands[1]);
        if (lhs && rhs) setSILValue(inst.result, builder.CreateFCmpOLE(lhs, rhs));
      } else if (name == "fcmp_oge_FPIEEE64") {
        auto* lhs = getSILValue(inst.operands[0]);
        auto* rhs = getSILValue(inst.operands[1]);
        if (lhs && rhs) setSILValue(inst.result, builder.CreateFCmpOGE(lhs, rhs));

      // Boolean ops.
      } else if (name == "xor_Int1") {
        auto* op = getSILValue(inst.operands[0]);
        if (op) setSILValue(inst.result,
                            builder.CreateXor(op, builder.getTrue()));
      } else if (name == "and_Int1") {
        auto* lhs = getSILValue(inst.operands[0]);
        auto* rhs = getSILValue(inst.operands[1]);
        if (lhs && rhs) setSILValue(inst.result, builder.CreateAnd(lhs, rhs));
      } else if (name == "or_Int1") {
        auto* lhs = getSILValue(inst.operands[0]);
        auto* rhs = getSILValue(inst.operands[1]);
        if (lhs && rhs) setSILValue(inst.result, builder.CreateOr(lhs, rhs));

      // Bitwise integer operations.
      } else if (name == "and_Int64") {
        auto* lhs = getSILValue(inst.operands[0]);
        auto* rhs = getSILValue(inst.operands[1]);
        if (lhs && rhs) setSILValue(inst.result, builder.CreateAnd(lhs, rhs));
      } else if (name == "or_Int64") {
        auto* lhs = getSILValue(inst.operands[0]);
        auto* rhs = getSILValue(inst.operands[1]);
        if (lhs && rhs) setSILValue(inst.result, builder.CreateOr(lhs, rhs));
      } else if (name == "xor_Int64") {
        auto* lhs = getSILValue(inst.operands[0]);
        auto* rhs = getSILValue(inst.operands[1]);
        if (lhs && rhs) setSILValue(inst.result, builder.CreateXor(lhs, rhs));
      } else if (name == "shl_Int64") {
        auto* lhs = getSILValue(inst.operands[0]);
        auto* rhs = getSILValue(inst.operands[1]);
        if (lhs && rhs) setSILValue(inst.result, builder.CreateShl(lhs, rhs));
      } else if (name == "ashr_Int64") {
        auto* lhs = getSILValue(inst.operands[0]);
        auto* rhs = getSILValue(inst.operands[1]);
        if (lhs && rhs) setSILValue(inst.result, builder.CreateAShr(lhs, rhs));
      } else if (name == "not_Int64") {
        auto* op = getSILValue(inst.operands[0]);
        if (op) setSILValue(inst.result, builder.CreateNot(op));

      // Conversions.
      } else if (name == "sitofp_Int64_FPIEEE64") {
        auto* op = getSILValue(inst.operands[0]);
        if (op) setSILValue(inst.result,
                            builder.CreateSIToFP(op, builder.getDoubleTy()));
      } else if (name == "fptosi_FPIEEE64_Int64") {
        auto* op = getSILValue(inst.operands[0]);
        if (op) setSILValue(inst.result,
                            builder.CreateFPToSI(op, builder.getInt64Ty()));

      // Array operations.
      } else if (name == "array_literal_init") {
        auto* elem_type = GetLLVMTypeForSIL(context, inst.alloc_type);
        if (!elem_type || elem_type->isVoidTy()) elem_type = builder.getInt64Ty();
        size_t count = inst.operand_list.size();
        if (count == 0) count = 1;  // Minimum 1-element alloca.
        auto* array_type = llvm::ArrayType::get(elem_type, count);
        auto* alloca = builder.CreateAlloca(array_type, nullptr, "arr");
        auto* zero = llvm::ConstantInt::get(builder.getInt64Ty(), 0);
        for (size_t i = 0; i < inst.operand_list.size(); ++i) {
          auto* elem_val = getSILValue(inst.operand_list[i]);
          if (!elem_val) continue;
          auto* idx = llvm::ConstantInt::get(builder.getInt64Ty(),
                                             static_cast<int64_t>(i));
          llvm::Value* indices[] = {zero, idx};
          auto* gep = builder.CreateGEP(array_type, alloca, indices);
          builder.CreateStore(elem_val, gep);
        }
        setSILValue(inst.result, alloca);
      } else if (name == "array_access") {
        auto* array_ptr = getSILValue(inst.operands[0]);
        auto* index_val = getSILValue(inst.operands[1]);
        if (array_ptr && index_val) {
          auto* elem_type = GetLLVMTypeForSIL(context, inst.alloc_type);
          if (!elem_type || elem_type->isVoidTy()) elem_type = builder.getInt64Ty();
          // GEP treats array_ptr as T* — element i is at ptr + i * sizeof(T).
          auto* gep = builder.CreateGEP(elem_type, array_ptr, {index_val});
          auto* loaded = builder.CreateLoad(elem_type, gep);
          setSILValue(inst.result, loaded);
        }
      } else if (name == "array_element_addr") {
        // GEP to produce a pointer to arr[index] for subsequent store.
        auto* array_ptr = getSILValue(inst.operands[0]);
        auto* index_val = getSILValue(inst.operands[1]);
        if (array_ptr && index_val) {
          auto* elem_type = GetLLVMTypeForSIL(context, inst.alloc_type);
          if (!elem_type || elem_type->isVoidTy()) elem_type = builder.getInt64Ty();
          auto* gep = builder.CreateGEP(elem_type, array_ptr, {index_val});
          setSILValue(inst.result, gep);
        }

      // M38: String operations.
      } else if (name == "string_concat") {
        auto* lhs = getSILValue(inst.operands[0]);
        auto* rhs = getSILValue(inst.operands[1]);
        if (lhs && rhs) {
          auto* fn_type = llvm::FunctionType::get(
              builder.getPtrTy(),
              {builder.getPtrTy(), builder.getPtrTy()}, false);
          auto callee = context.module().getOrInsertFunction(
              "__tinyswift_string_concat", fn_type);
          setSILValue(inst.result, builder.CreateCall(callee, {lhs, rhs}));
        } else {
          llvm::errs() << "WARNING: string_concat: null operand\n";
        }
      } else if (name == "string_len") {
        // M49: string_len(s) → int64_t strlen(s)
        auto* str_ptr = getSILValue(inst.operands[0]);
        if (str_ptr) {
          auto* fty = llvm::FunctionType::get(builder.getInt64Ty(),
                                              {builder.getPtrTy()}, false);
          auto callee = context.module().getOrInsertFunction(
              "__tinyswift_string_len", fty);
          setSILValue(inst.result, builder.CreateCall(callee, {str_ptr}));
        }
      } else if (name == "int_to_string") {
        auto* val = getSILValue(inst.operands[0]);
        if (val) {
          // Widen to i64 if needed (e.g. i1 from Bool).
          if (!val->getType()->isIntegerTy(64)) {
            val = builder.CreateSExt(val, builder.getInt64Ty());
          }
          auto* fn_type = llvm::FunctionType::get(
              builder.getPtrTy(), {builder.getInt64Ty()}, false);
          auto callee = context.module().getOrInsertFunction(
              "__tinyswift_int_to_string", fn_type);
          setSILValue(inst.result, builder.CreateCall(callee, {val}));
        }

      // M44: print() built-in.
      } else if (name == "print_int") {
        auto* val = getSILValue(inst.operands[0]);
        if (val) {
          // Widen to i64 if needed (e.g. i1 from Bool).
          if (!val->getType()->isIntegerTy(64)) {
            val = builder.CreateSExt(val, builder.getInt64Ty());
          }
          auto* fn_type = llvm::FunctionType::get(
              builder.getVoidTy(), {builder.getInt64Ty()}, false);
          auto callee = context.module().getOrInsertFunction(
              "__tinyswift_print_int", fn_type);
          builder.CreateCall(callee, {val});
        }
      } else if (name == "print_string") {
        auto* val = getSILValue(inst.operands[0]);
        if (val) {
          auto* fn_type = llvm::FunctionType::get(
              builder.getVoidTy(), {builder.getPtrTy()}, false);
          auto callee = context.module().getOrInsertFunction(
              "__tinyswift_print_string", fn_type);
          builder.CreateCall(callee, {val});
        }

      // M92: File I/O builtins.
      } else if (name == "readline") {
        auto* fty = llvm::FunctionType::get(builder.getPtrTy(), {}, false);
        auto callee = context.module().getOrInsertFunction(
            "__tinyswift_readline", fty);
        setSILValue(inst.result, builder.CreateCall(callee, {}));
      } else if (name == "file_getcwd") {
        auto* fty = llvm::FunctionType::get(builder.getPtrTy(), {}, false);
        auto callee = context.module().getOrInsertFunction(
            "__tinyswift_file_getcwd", fty);
        setSILValue(inst.result, builder.CreateCall(callee, {}));
      } else if (name == "file_read_all") {
        auto* path_ptr = getSILValue(inst.operands[0]);
        if (path_ptr) {
          auto* fty = llvm::FunctionType::get(builder.getPtrTy(),
                                              {builder.getPtrTy()}, false);
          auto callee = context.module().getOrInsertFunction(
              "__tinyswift_file_read_all", fty);
          setSILValue(inst.result, builder.CreateCall(callee, {path_ptr}));
        }
      } else if (name == "file_exists") {
        auto* path_ptr = getSILValue(inst.operands[0]);
        if (path_ptr) {
          auto* fty = llvm::FunctionType::get(builder.getInt64Ty(),
                                              {builder.getPtrTy()}, false);
          auto callee = context.module().getOrInsertFunction(
              "__tinyswift_file_exists", fty);
          setSILValue(inst.result, builder.CreateCall(callee, {path_ptr}));
        }
      } else if (name == "file_remove") {
        auto* path_ptr = getSILValue(inst.operands[0]);
        if (path_ptr) {
          auto* fty = llvm::FunctionType::get(builder.getInt64Ty(),
                                              {builder.getPtrTy()}, false);
          auto callee = context.module().getOrInsertFunction(
              "__tinyswift_file_remove", fty);
          setSILValue(inst.result, builder.CreateCall(callee, {path_ptr}));
        }
      } else if (name == "file_write_all") {
        auto* path_ptr = getSILValue(inst.operands[0]);
        auto* data_ptr = getSILValue(inst.operands[1]);
        if (path_ptr && data_ptr) {
          auto* fty = llvm::FunctionType::get(
              builder.getInt64Ty(),
              {builder.getPtrTy(), builder.getPtrTy()}, false);
          auto callee = context.module().getOrInsertFunction(
              "__tinyswift_file_write_all", fty);
          setSILValue(inst.result, builder.CreateCall(callee, {path_ptr, data_ptr}));
        }
      } else if (name == "file_append_all") {
        auto* path_ptr = getSILValue(inst.operands[0]);
        auto* data_ptr = getSILValue(inst.operands[1]);
        if (path_ptr && data_ptr) {
          auto* fty = llvm::FunctionType::get(
              builder.getInt64Ty(),
              {builder.getPtrTy(), builder.getPtrTy()}, false);
          auto callee = context.module().getOrInsertFunction(
              "__tinyswift_file_append_all", fty);
          setSILValue(inst.result, builder.CreateCall(callee, {path_ptr, data_ptr}));
        }

      // M93: OS — Process & Environment.
      } else if (name == "process_get_args") {
        auto* fty = llvm::FunctionType::get(builder.getPtrTy(), {}, false);
        auto callee = context.module().getOrInsertFunction(
            "__tinyswift_get_args", fty);
        setSILValue(inst.result, builder.CreateCall(callee, {}));
      } else if (name == "process_exit") {
        auto* code_val = getSILValue(inst.operands[0]);
        if (code_val) {
          auto* fty = llvm::FunctionType::get(
              builder.getVoidTy(), {builder.getInt64Ty()}, false);
          auto callee = context.module().getOrInsertFunction(
              "__tinyswift_exit", fty);
          builder.CreateCall(callee, {code_val});
        }
      } else if (name == "env_get") {
        auto* key_ptr = getSILValue(inst.operands[0]);
        if (key_ptr) {
          auto* fty = llvm::FunctionType::get(builder.getPtrTy(),
                                              {builder.getPtrTy()}, false);
          auto callee = context.module().getOrInsertFunction(
              "__tinyswift_env_get", fty);
          setSILValue(inst.result, builder.CreateCall(callee, {key_ptr}));
        }
      } else if (name == "env_set") {
        auto* key_ptr = getSILValue(inst.operands[0]);
        auto* val_ptr = getSILValue(inst.operands[1]);
        if (key_ptr && val_ptr) {
          auto* fty = llvm::FunctionType::get(
              builder.getInt64Ty(),
              {builder.getPtrTy(), builder.getPtrTy()}, false);
          auto callee = context.module().getOrInsertFunction(
              "__tinyswift_env_set", fty);
          setSILValue(inst.result, builder.CreateCall(callee, {key_ptr, val_ptr}));
        }

      // M93: OS — FileSystem extensions.
      } else if (name == "fs_mkdir") {
        auto* path_ptr = getSILValue(inst.operands[0]);
        if (path_ptr) {
          auto* fty = llvm::FunctionType::get(builder.getInt64Ty(),
                                              {builder.getPtrTy()}, false);
          auto callee = context.module().getOrInsertFunction(
              "__tinyswift_fs_mkdir", fty);
          setSILValue(inst.result, builder.CreateCall(callee, {path_ptr}));
        }
      } else if (name == "fs_listdir") {
        auto* path_ptr = getSILValue(inst.operands[0]);
        if (path_ptr) {
          auto* fty = llvm::FunctionType::get(builder.getPtrTy(),
                                              {builder.getPtrTy()}, false);
          auto callee = context.module().getOrInsertFunction(
              "__tinyswift_fs_listdir", fty);
          setSILValue(inst.result, builder.CreateCall(callee, {path_ptr}));
        }
      } else if (name == "fs_is_dir") {
        auto* path_ptr = getSILValue(inst.operands[0]);
        if (path_ptr) {
          auto* fty = llvm::FunctionType::get(builder.getInt64Ty(),
                                              {builder.getPtrTy()}, false);
          auto callee = context.module().getOrInsertFunction(
              "__tinyswift_fs_is_dir", fty);
          setSILValue(inst.result, builder.CreateCall(callee, {path_ptr}));
        }
      } else if (name == "fs_copy") {
        auto* src_ptr = getSILValue(inst.operands[0]);
        auto* dst_ptr = getSILValue(inst.operands[1]);
        if (src_ptr && dst_ptr) {
          auto* fty = llvm::FunctionType::get(
              builder.getInt64Ty(),
              {builder.getPtrTy(), builder.getPtrTy()}, false);
          auto callee = context.module().getOrInsertFunction(
              "__tinyswift_fs_copy", fty);
          setSILValue(inst.result, builder.CreateCall(callee, {src_ptr, dst_ptr}));
        }

      // M94: Networking — TCP Sockets.
      } else if (name == "tcp_connect") {
        auto* host_ptr = getSILValue(inst.operands[0]);
        auto* port_val = getSILValue(inst.operands[1]);
        if (host_ptr && port_val) {
          auto* fty = llvm::FunctionType::get(
              builder.getInt64Ty(),
              {builder.getPtrTy(), builder.getInt64Ty()}, false);
          auto callee = context.module().getOrInsertFunction(
              "__tinyswift_tcp_connect", fty);
          setSILValue(inst.result, builder.CreateCall(callee, {host_ptr, port_val}));
        }
      } else if (name == "tcp_listen") {
        auto* port_val = getSILValue(inst.operands[0]);
        if (port_val) {
          auto* fty = llvm::FunctionType::get(builder.getInt64Ty(),
                                              {builder.getInt64Ty()}, false);
          auto callee = context.module().getOrInsertFunction(
              "__tinyswift_tcp_listen", fty);
          setSILValue(inst.result, builder.CreateCall(callee, {port_val}));
        }
      } else if (name == "tcp_accept") {
        auto* fd_val = getSILValue(inst.operands[0]);
        if (fd_val) {
          auto* fty = llvm::FunctionType::get(builder.getInt64Ty(),
                                              {builder.getInt64Ty()}, false);
          auto callee = context.module().getOrInsertFunction(
              "__tinyswift_tcp_accept", fty);
          setSILValue(inst.result, builder.CreateCall(callee, {fd_val}));
        }
      } else if (name == "tcp_read") {
        auto* fd_val = getSILValue(inst.operands[0]);
        auto* maxlen_val = getSILValue(inst.operands[1]);
        if (fd_val && maxlen_val) {
          auto* fty = llvm::FunctionType::get(
              builder.getPtrTy(),
              {builder.getInt64Ty(), builder.getInt64Ty()}, false);
          auto callee = context.module().getOrInsertFunction(
              "__tinyswift_tcp_read", fty);
          setSILValue(inst.result, builder.CreateCall(callee, {fd_val, maxlen_val}));
        }
      } else if (name == "tcp_write") {
        auto* fd_val = getSILValue(inst.operands[0]);
        auto* data_ptr = getSILValue(inst.operands[1]);
        if (fd_val && data_ptr) {
          auto* fty = llvm::FunctionType::get(
              builder.getInt64Ty(),
              {builder.getInt64Ty(), builder.getPtrTy()}, false);
          auto callee = context.module().getOrInsertFunction(
              "__tinyswift_tcp_write", fty);
          setSILValue(inst.result, builder.CreateCall(callee, {fd_val, data_ptr}));
        }
      } else if (name == "tcp_close") {
        auto* fd_val = getSILValue(inst.operands[0]);
        if (fd_val) {
          auto* fty = llvm::FunctionType::get(builder.getInt64Ty(),
                                              {builder.getInt64Ty()}, false);
          auto callee = context.module().getOrInsertFunction(
              "__tinyswift_tcp_close", fty);
          setSILValue(inst.result, builder.CreateCall(callee, {fd_val}));
        }

      // M45: String equality.
      } else if (name == "string_eq") {
        auto* lhs = getSILValue(inst.operands[0]);
        auto* rhs = getSILValue(inst.operands[1]);
        if (lhs && rhs) {
          auto* fn_type = llvm::FunctionType::get(
              builder.getInt1Ty(),
              {builder.getPtrTy(), builder.getPtrTy()}, false);
          auto callee = context.module().getOrInsertFunction(
              "__tinyswift_string_eq", fn_type);
          auto* cmp = builder.CreateCall(callee, {lhs, rhs});
          // Zext to i64 for Bool storage convention.
          setSILValue(inst.result, builder.CreateZExt(cmp, builder.getInt64Ty()));
        }
      } else if (name == "string_neq") {
        auto* lhs = getSILValue(inst.operands[0]);
        auto* rhs = getSILValue(inst.operands[1]);
        if (lhs && rhs) {
          auto* fn_type = llvm::FunctionType::get(
              builder.getInt1Ty(),
              {builder.getPtrTy(), builder.getPtrTy()}, false);
          auto callee = context.module().getOrInsertFunction(
              "__tinyswift_string_eq", fn_type);
          auto* cmp = builder.CreateCall(callee, {lhs, rhs});
          // Invert and zext to i64.
          auto* inv = builder.CreateNot(cmp);
          setSILValue(inst.result, builder.CreateZExt(inv, builder.getInt64Ty()));
        }

      // M42/M91: Dictionary operations (upgraded to hashmap backend).
      } else if (name == "dict_init") {
        // M91: Create hashmap and insert all literal entries.
        // literal_value = number of key-value pairs.
        // operand_list = interleaved [k0,v0,k1,v1,...].
        int64_t count = inst.literal_value;

        // Determine key/val types from operands.
        llvm::Type* key_llvm_type = builder.getPtrTy();  // default: String
        llvm::Type* val_llvm_type = builder.getInt64Ty(); // default: Int
        bool key_is_string = true;

        if (count > 0 && inst.operand_list.size() >= 2) {
          auto* k0 = getSILValue(inst.operand_list[0]);
          auto* v0 = getSILValue(inst.operand_list[1]);
          if (k0) key_llvm_type = k0->getType();
          if (v0) val_llvm_type = v0->getType();
          key_is_string = key_llvm_type->isPointerTy();
        }

        auto& layout = context.module().getDataLayout();
        uint64_t key_size = layout.getTypeAllocSize(key_llvm_type);
        uint64_t val_size = layout.getTypeAllocSize(val_llvm_type);

        // Get eq function.
        const char* eq_fn_name = key_is_string
            ? "__tinyswift_eq_string" : "__tinyswift_eq_int";
        auto* eq_fty = llvm::FunctionType::get(
            builder.getInt64Ty(),
            {builder.getPtrTy(), builder.getPtrTy()}, false);
        auto eq_fn = context.module().getOrInsertFunction(eq_fn_name, eq_fty);

        // Create hashmap.
        auto* create_fty = llvm::FunctionType::get(
            builder.getPtrTy(),
            {builder.getInt64Ty(), builder.getInt64Ty(), builder.getPtrTy()},
            false);
        auto create_fn = context.module().getOrInsertFunction(
            "__tinyswift_hashmap_create", create_fty);
        auto* map_ptr = builder.CreateCall(create_fn, {
            llvm::ConstantInt::get(builder.getInt64Ty(), key_size),
            llvm::ConstantInt::get(builder.getInt64Ty(), val_size),
            eq_fn.getCallee()});

        // Insert each k/v pair.
        auto* set_fty = llvm::FunctionType::get(
            builder.getVoidTy(),
            {builder.getPtrTy(), builder.getPtrTy(),
             builder.getInt64Ty(), builder.getPtrTy()}, false);
        auto set_fn = context.module().getOrInsertFunction(
            "__tinyswift_hashmap_set", set_fty);

        // Hash function name.
        const char* hash_fn_name = key_is_string
            ? "__tinyswift_string_hash" : "__tinyswift_int_hash";

        for (int64_t i = 0; i < count; ++i) {
          size_t ki = static_cast<size_t>(i * 2);
          size_t vi = static_cast<size_t>(i * 2 + 1);
          if (ki >= inst.operand_list.size() ||
              vi >= inst.operand_list.size()) break;
          auto* k = getSILValue(inst.operand_list[ki]);
          auto* v = getSILValue(inst.operand_list[vi]);
          if (!k || !v) continue;

          auto* key_alloca = builder.CreateAlloca(key_llvm_type);
          builder.CreateStore(k, key_alloca);
          auto* val_alloca = builder.CreateAlloca(val_llvm_type);
          if (v->getType() != val_llvm_type && v->getType()->isIntegerTy() &&
              val_llvm_type->isIntegerTy()) {
            v = builder.CreateSExtOrTrunc(v, val_llvm_type);
          }
          builder.CreateStore(v, val_alloca);

          // Compute hash.
          llvm::Value* hash_val = nullptr;
          if (key_is_string) {
            auto* hfty = llvm::FunctionType::get(
                builder.getInt64Ty(), {builder.getPtrTy()}, false);
            auto hfn = context.module().getOrInsertFunction(hash_fn_name, hfty);
            hash_val = builder.CreateCall(hfn, {k});
          } else {
            auto* hfty = llvm::FunctionType::get(
                builder.getInt64Ty(), {builder.getInt64Ty()}, false);
            auto hfn = context.module().getOrInsertFunction(hash_fn_name, hfty);
            auto* k_i64 = k;
            if (!k->getType()->isIntegerTy(64)) {
              k_i64 = builder.CreateSExt(k, builder.getInt64Ty());
            }
            hash_val = builder.CreateCall(hfn, {k_i64});
          }

          builder.CreateCall(set_fn, {map_ptr, key_alloca, hash_val, val_alloca});
        }
        setSILValue(inst.result, map_ptr);

      } else if (name == "dict_access") {
        // M42 compat: dict_access still uses old runtime.
        auto* dict_ptr = getSILValue(inst.operands[0]);
        auto* key_ptr  = getSILValue(inst.operands[1]);
        if (dict_ptr && key_ptr) {
          llvm::SmallVector<llvm::Type*, 2> opt_fields = {builder.getInt1Ty(),
                                                           builder.getInt64Ty()};
          auto* opt_ty = llvm::StructType::get(context.llvm_context(),
                                               llvm::ArrayRef<llvm::Type*>(opt_fields));
          auto* fn_type = llvm::FunctionType::get(
              opt_ty, {builder.getPtrTy(), builder.getPtrTy()}, false);
          auto callee = context.module().getOrInsertFunction(
              "__tinyswift_dict_get_str_int", fn_type);
          setSILValue(inst.result, builder.CreateCall(callee, {dict_ptr, key_ptr}));
        }

      // M91: Generic hash map operations.
      } else if (name == "hashmap_create") {
        // literal_value = (key_type_idx << 32) | (val_type_idx & 0xFFFFFFFF).
        int32_t key_type_idx = static_cast<int32_t>(inst.literal_value >> 32);
        int32_t val_type_idx = static_cast<int32_t>(inst.literal_value & 0xFFFFFFFF);
        auto key_type_id = SemIR::TypeId(key_type_idx);
        auto val_type_id = SemIR::TypeId(val_type_idx);
        auto* key_llvm_type2 = context.GetType(key_type_id);
        auto* val_llvm_type2 = context.GetType(val_type_id);
        auto& layout2 = context.module().getDataLayout();
        uint64_t key_sz = layout2.getTypeAllocSize(key_llvm_type2);
        uint64_t val_sz = layout2.getTypeAllocSize(val_llvm_type2);

        // Select eq function based on key type.
        const char* eq_name = "__tinyswift_eq_int";
        if (key_llvm_type2->isPointerTy()) eq_name = "__tinyswift_eq_string";
        else if (key_llvm_type2->isDoubleTy()) eq_name = "__tinyswift_eq_double";
        else if (key_llvm_type2->isIntegerTy(1)) eq_name = "__tinyswift_eq_bool";

        auto* eq_fty2 = llvm::FunctionType::get(
            builder.getInt64Ty(),
            {builder.getPtrTy(), builder.getPtrTy()}, false);
        auto eq_fn2 = context.module().getOrInsertFunction(eq_name, eq_fty2);

        auto* create_fty2 = llvm::FunctionType::get(
            builder.getPtrTy(),
            {builder.getInt64Ty(), builder.getInt64Ty(), builder.getPtrTy()},
            false);
        auto create_fn2 = context.module().getOrInsertFunction(
            "__tinyswift_hashmap_create", create_fty2);
        setSILValue(inst.result, builder.CreateCall(create_fn2, {
            llvm::ConstantInt::get(builder.getInt64Ty(), key_sz),
            llvm::ConstantInt::get(builder.getInt64Ty(), val_sz),
            eq_fn2.getCallee()}));

      } else if (name == "hashmap_set") {
        // operands: [0]=map, [1]=key (in operand_list), [2]=val (in operand_list)
        auto* map_ptr = getSILValue(inst.operands[0]);
        llvm::Value* key_val = nullptr;
        llvm::Value* val_val = nullptr;
        if (inst.operand_list.size() >= 1)
          key_val = getSILValue(inst.operand_list[0]);
        if (inst.operand_list.size() >= 2)
          val_val = getSILValue(inst.operand_list[1]);
        if (map_ptr && key_val && val_val) {
          auto* key_alloca = builder.CreateAlloca(key_val->getType());
          builder.CreateStore(key_val, key_alloca);
          auto* val_alloca = builder.CreateAlloca(val_val->getType());
          builder.CreateStore(val_val, val_alloca);

          // Compute hash.
          llvm::Value* hash_val = nullptr;
          bool is_string_key = key_val->getType()->isPointerTy();
          if (is_string_key) {
            auto* hfty = llvm::FunctionType::get(
                builder.getInt64Ty(), {builder.getPtrTy()}, false);
            auto hfn = context.module().getOrInsertFunction(
                "__tinyswift_string_hash", hfty);
            hash_val = builder.CreateCall(hfn, {key_val});
          } else if (key_val->getType()->isDoubleTy()) {
            auto* hfty = llvm::FunctionType::get(
                builder.getInt64Ty(), {builder.getDoubleTy()}, false);
            auto hfn = context.module().getOrInsertFunction(
                "__tinyswift_double_hash", hfty);
            hash_val = builder.CreateCall(hfn, {key_val});
          } else {
            auto* hfty = llvm::FunctionType::get(
                builder.getInt64Ty(), {builder.getInt64Ty()}, false);
            auto hfn = context.module().getOrInsertFunction(
                "__tinyswift_int_hash", hfty);
            auto* k_i64 = key_val;
            if (!key_val->getType()->isIntegerTy(64))
              k_i64 = builder.CreateSExt(key_val, builder.getInt64Ty());
            hash_val = builder.CreateCall(hfn, {k_i64});
          }

          auto* set_fty = llvm::FunctionType::get(
              builder.getVoidTy(),
              {builder.getPtrTy(), builder.getPtrTy(),
               builder.getInt64Ty(), builder.getPtrTy()}, false);
          auto set_fn = context.module().getOrInsertFunction(
              "__tinyswift_hashmap_set", set_fty);
          builder.CreateCall(set_fn, {map_ptr, key_alloca, hash_val, val_alloca});
        }

      } else if (name == "hashmap_get") {
        // operands: [0]=map, [1]=key. literal_value = Optional<V> type_id.
        auto* map_ptr = getSILValue(inst.operands[0]);
        auto* key_val = getSILValue(inst.operands[1]);
        if (map_ptr && key_val) {
          // Determine val LLVM type from the Optional type.
          // The result is Optional<V> = {i1, V_type}.
          // For now, default to i64 (Int).
          llvm::Type* val_llvm_type3 = builder.getInt64Ty();

          auto* key_alloca = builder.CreateAlloca(key_val->getType());
          builder.CreateStore(key_val, key_alloca);
          auto* val_alloca = builder.CreateAlloca(val_llvm_type3);

          // Compute hash.
          llvm::Value* hash_val = nullptr;
          bool is_string_key = key_val->getType()->isPointerTy();
          if (is_string_key) {
            auto* hfty = llvm::FunctionType::get(
                builder.getInt64Ty(), {builder.getPtrTy()}, false);
            auto hfn = context.module().getOrInsertFunction(
                "__tinyswift_string_hash", hfty);
            hash_val = builder.CreateCall(hfn, {key_val});
          } else if (key_val->getType()->isDoubleTy()) {
            auto* hfty = llvm::FunctionType::get(
                builder.getInt64Ty(), {builder.getDoubleTy()}, false);
            auto hfn = context.module().getOrInsertFunction(
                "__tinyswift_double_hash", hfty);
            hash_val = builder.CreateCall(hfn, {key_val});
          } else {
            auto* hfty = llvm::FunctionType::get(
                builder.getInt64Ty(), {builder.getInt64Ty()}, false);
            auto hfn = context.module().getOrInsertFunction(
                "__tinyswift_int_hash", hfty);
            auto* k_i64 = key_val;
            if (!key_val->getType()->isIntegerTy(64))
              k_i64 = builder.CreateSExt(key_val, builder.getInt64Ty());
            hash_val = builder.CreateCall(hfn, {k_i64});
          }

          auto* get_fty = llvm::FunctionType::get(
              builder.getInt64Ty(),
              {builder.getPtrTy(), builder.getPtrTy(),
               builder.getInt64Ty(), builder.getPtrTy()}, false);
          auto get_fn = context.module().getOrInsertFunction(
              "__tinyswift_hashmap_get", get_fty);
          auto* found = builder.CreateCall(
              get_fn, {map_ptr, key_alloca, hash_val, val_alloca});
          auto* found_i1 = builder.CreateTrunc(found, builder.getInt1Ty());
          auto* val_loaded = builder.CreateLoad(val_llvm_type3, val_alloca);

          // Build Optional struct {i1, val_type}.
          llvm::SmallVector<llvm::Type*, 2> opt_fields = {
              builder.getInt1Ty(), val_llvm_type3};
          auto* opt_ty = llvm::StructType::get(context.llvm_context(),
                                               llvm::ArrayRef<llvm::Type*>(opt_fields));
          llvm::Value* opt_val = llvm::UndefValue::get(opt_ty);
          opt_val = builder.CreateInsertValue(opt_val, found_i1, {0});
          opt_val = builder.CreateInsertValue(opt_val, val_loaded, {1});
          setSILValue(inst.result, opt_val);
        }

      } else if (name == "hashmap_count") {
        auto* map_ptr = getSILValue(inst.operands[0]);
        if (map_ptr) {
          auto* fty = llvm::FunctionType::get(builder.getInt64Ty(),
                                              {builder.getPtrTy()}, false);
          auto callee = context.module().getOrInsertFunction(
              "__tinyswift_hashmap_count", fty);
          setSILValue(inst.result, builder.CreateCall(callee, {map_ptr}));
        }

      } else if (name == "hashmap_contains") {
        auto* map_ptr = getSILValue(inst.operands[0]);
        auto* key_val = getSILValue(inst.operands[1]);
        if (map_ptr && key_val) {
          auto* key_alloca = builder.CreateAlloca(key_val->getType());
          builder.CreateStore(key_val, key_alloca);

          llvm::Value* hash_val = nullptr;
          bool is_string_key = key_val->getType()->isPointerTy();
          if (is_string_key) {
            auto* hfty = llvm::FunctionType::get(
                builder.getInt64Ty(), {builder.getPtrTy()}, false);
            auto hfn = context.module().getOrInsertFunction(
                "__tinyswift_string_hash", hfty);
            hash_val = builder.CreateCall(hfn, {key_val});
          } else {
            auto* hfty = llvm::FunctionType::get(
                builder.getInt64Ty(), {builder.getInt64Ty()}, false);
            auto hfn = context.module().getOrInsertFunction(
                "__tinyswift_int_hash", hfty);
            auto* k_i64 = key_val;
            if (!key_val->getType()->isIntegerTy(64))
              k_i64 = builder.CreateSExt(key_val, builder.getInt64Ty());
            hash_val = builder.CreateCall(hfn, {k_i64});
          }

          auto* fty = llvm::FunctionType::get(
              builder.getInt64Ty(),
              {builder.getPtrTy(), builder.getPtrTy(), builder.getInt64Ty()},
              false);
          auto callee = context.module().getOrInsertFunction(
              "__tinyswift_hashmap_contains", fty);
          setSILValue(inst.result,
                      builder.CreateCall(callee, {map_ptr, key_alloca, hash_val}));
        }

      } else if (name == "hashmap_remove") {
        auto* map_ptr = getSILValue(inst.operands[0]);
        auto* key_val = getSILValue(inst.operands[1]);
        if (map_ptr && key_val) {
          auto* key_alloca = builder.CreateAlloca(key_val->getType());
          builder.CreateStore(key_val, key_alloca);

          llvm::Value* hash_val = nullptr;
          bool is_string_key = key_val->getType()->isPointerTy();
          if (is_string_key) {
            auto* hfty = llvm::FunctionType::get(
                builder.getInt64Ty(), {builder.getPtrTy()}, false);
            auto hfn = context.module().getOrInsertFunction(
                "__tinyswift_string_hash", hfty);
            hash_val = builder.CreateCall(hfn, {key_val});
          } else {
            auto* hfty = llvm::FunctionType::get(
                builder.getInt64Ty(), {builder.getInt64Ty()}, false);
            auto hfn = context.module().getOrInsertFunction(
                "__tinyswift_int_hash", hfty);
            auto* k_i64 = key_val;
            if (!key_val->getType()->isIntegerTy(64))
              k_i64 = builder.CreateSExt(key_val, builder.getInt64Ty());
            hash_val = builder.CreateCall(hfn, {k_i64});
          }

          auto* fty = llvm::FunctionType::get(
              builder.getVoidTy(),
              {builder.getPtrTy(), builder.getPtrTy(), builder.getInt64Ty()},
              false);
          auto callee = context.module().getOrInsertFunction(
              "__tinyswift_hashmap_remove", fty);
          builder.CreateCall(callee, {map_ptr, key_alloca, hash_val});
        }

      // M91: Generic hash set operations.
      } else if (name == "hashset_create") {
        // literal_value = elem TypeId index.
        auto elem_type_id = SemIR::TypeId(static_cast<int32_t>(inst.literal_value));
        auto* elem_llvm_type = context.GetType(elem_type_id);
        auto& layout3 = context.module().getDataLayout();
        uint64_t elem_sz = layout3.getTypeAllocSize(elem_llvm_type);

        const char* eq_name3 = "__tinyswift_eq_int";
        if (elem_llvm_type->isPointerTy()) eq_name3 = "__tinyswift_eq_string";
        else if (elem_llvm_type->isDoubleTy()) eq_name3 = "__tinyswift_eq_double";
        else if (elem_llvm_type->isIntegerTy(1)) eq_name3 = "__tinyswift_eq_bool";

        auto* eq_fty3 = llvm::FunctionType::get(
            builder.getInt64Ty(),
            {builder.getPtrTy(), builder.getPtrTy()}, false);
        auto eq_fn3 = context.module().getOrInsertFunction(eq_name3, eq_fty3);

        auto* fty = llvm::FunctionType::get(
            builder.getPtrTy(),
            {builder.getInt64Ty(), builder.getPtrTy()}, false);
        auto callee = context.module().getOrInsertFunction(
            "__tinyswift_hashset_create", fty);
        setSILValue(inst.result, builder.CreateCall(callee, {
            llvm::ConstantInt::get(builder.getInt64Ty(), elem_sz),
            eq_fn3.getCallee()}));

      } else if (name == "hashset_insert") {
        auto* set_ptr = getSILValue(inst.operands[0]);
        auto* elem_val = getSILValue(inst.operands[1]);
        if (set_ptr && elem_val) {
          auto* elem_alloca = builder.CreateAlloca(elem_val->getType());
          builder.CreateStore(elem_val, elem_alloca);

          llvm::Value* hash_val = nullptr;
          if (elem_val->getType()->isPointerTy()) {
            auto* hfty = llvm::FunctionType::get(
                builder.getInt64Ty(), {builder.getPtrTy()}, false);
            auto hfn = context.module().getOrInsertFunction(
                "__tinyswift_string_hash", hfty);
            hash_val = builder.CreateCall(hfn, {elem_val});
          } else if (elem_val->getType()->isDoubleTy()) {
            auto* hfty = llvm::FunctionType::get(
                builder.getInt64Ty(), {builder.getDoubleTy()}, false);
            auto hfn = context.module().getOrInsertFunction(
                "__tinyswift_double_hash", hfty);
            hash_val = builder.CreateCall(hfn, {elem_val});
          } else {
            auto* hfty = llvm::FunctionType::get(
                builder.getInt64Ty(), {builder.getInt64Ty()}, false);
            auto hfn = context.module().getOrInsertFunction(
                "__tinyswift_int_hash", hfty);
            auto* e_i64 = elem_val;
            if (!elem_val->getType()->isIntegerTy(64))
              e_i64 = builder.CreateSExt(elem_val, builder.getInt64Ty());
            hash_val = builder.CreateCall(hfn, {e_i64});
          }

          auto* fty = llvm::FunctionType::get(
              builder.getVoidTy(),
              {builder.getPtrTy(), builder.getPtrTy(), builder.getInt64Ty()},
              false);
          auto callee = context.module().getOrInsertFunction(
              "__tinyswift_hashset_insert", fty);
          builder.CreateCall(callee, {set_ptr, elem_alloca, hash_val});
        }

      } else if (name == "hashset_contains") {
        auto* set_ptr = getSILValue(inst.operands[0]);
        auto* elem_val = getSILValue(inst.operands[1]);
        if (set_ptr && elem_val) {
          auto* elem_alloca = builder.CreateAlloca(elem_val->getType());
          builder.CreateStore(elem_val, elem_alloca);

          llvm::Value* hash_val = nullptr;
          if (elem_val->getType()->isPointerTy()) {
            auto* hfty = llvm::FunctionType::get(
                builder.getInt64Ty(), {builder.getPtrTy()}, false);
            auto hfn = context.module().getOrInsertFunction(
                "__tinyswift_string_hash", hfty);
            hash_val = builder.CreateCall(hfn, {elem_val});
          } else if (elem_val->getType()->isDoubleTy()) {
            auto* hfty = llvm::FunctionType::get(
                builder.getInt64Ty(), {builder.getDoubleTy()}, false);
            auto hfn = context.module().getOrInsertFunction(
                "__tinyswift_double_hash", hfty);
            hash_val = builder.CreateCall(hfn, {elem_val});
          } else {
            auto* hfty = llvm::FunctionType::get(
                builder.getInt64Ty(), {builder.getInt64Ty()}, false);
            auto hfn = context.module().getOrInsertFunction(
                "__tinyswift_int_hash", hfty);
            auto* e_i64 = elem_val;
            if (!elem_val->getType()->isIntegerTy(64))
              e_i64 = builder.CreateSExt(elem_val, builder.getInt64Ty());
            hash_val = builder.CreateCall(hfn, {e_i64});
          }

          auto* fty = llvm::FunctionType::get(
              builder.getInt64Ty(),
              {builder.getPtrTy(), builder.getPtrTy(), builder.getInt64Ty()},
              false);
          auto callee = context.module().getOrInsertFunction(
              "__tinyswift_hashset_contains", fty);
          setSILValue(inst.result,
                      builder.CreateCall(callee, {set_ptr, elem_alloca, hash_val}));
        }

      } else if (name == "hashset_count") {
        auto* set_ptr = getSILValue(inst.operands[0]);
        if (set_ptr) {
          auto* fty = llvm::FunctionType::get(builder.getInt64Ty(),
                                              {builder.getPtrTy()}, false);
          auto callee = context.module().getOrInsertFunction(
              "__tinyswift_hashset_count", fty);
          setSILValue(inst.result, builder.CreateCall(callee, {set_ptr}));
        }

      } else if (name == "hashset_remove") {
        auto* set_ptr = getSILValue(inst.operands[0]);
        auto* elem_val = getSILValue(inst.operands[1]);
        if (set_ptr && elem_val) {
          auto* elem_alloca = builder.CreateAlloca(elem_val->getType());
          builder.CreateStore(elem_val, elem_alloca);

          llvm::Value* hash_val = nullptr;
          if (elem_val->getType()->isPointerTy()) {
            auto* hfty = llvm::FunctionType::get(
                builder.getInt64Ty(), {builder.getPtrTy()}, false);
            auto hfn = context.module().getOrInsertFunction(
                "__tinyswift_string_hash", hfty);
            hash_val = builder.CreateCall(hfn, {elem_val});
          } else {
            auto* hfty = llvm::FunctionType::get(
                builder.getInt64Ty(), {builder.getInt64Ty()}, false);
            auto hfn = context.module().getOrInsertFunction(
                "__tinyswift_int_hash", hfty);
            auto* e_i64 = elem_val;
            if (!elem_val->getType()->isIntegerTy(64))
              e_i64 = builder.CreateSExt(elem_val, builder.getInt64Ty());
            hash_val = builder.CreateCall(hfn, {e_i64});
          }

          auto* fty = llvm::FunctionType::get(
              builder.getVoidTy(),
              {builder.getPtrTy(), builder.getPtrTy(), builder.getInt64Ty()},
              false);
          auto callee = context.module().getOrInsertFunction(
              "__tinyswift_hashset_remove", fty);
          builder.CreateCall(callee, {set_ptr, elem_alloca, hash_val});
        }

      // M61: String method calls.
      } else if (name == "string_uppercased") {
        auto* str_ptr = getSILValue(inst.operands[0]);
        if (str_ptr) {
          auto* fty = llvm::FunctionType::get(builder.getPtrTy(),
                                              {builder.getPtrTy()}, false);
          auto callee = context.module().getOrInsertFunction(
              "__tinyswift_string_uppercased", fty);
          setSILValue(inst.result, builder.CreateCall(callee, {str_ptr}));
        }
      } else if (name == "string_lowercased") {
        auto* str_ptr = getSILValue(inst.operands[0]);
        if (str_ptr) {
          auto* fty = llvm::FunctionType::get(builder.getPtrTy(),
                                              {builder.getPtrTy()}, false);
          auto callee = context.module().getOrInsertFunction(
              "__tinyswift_string_lowercased", fty);
          setSILValue(inst.result, builder.CreateCall(callee, {str_ptr}));
        }
      } else if (name == "string_trimmed") {
        auto* str_ptr = getSILValue(inst.operands[0]);
        if (str_ptr) {
          auto* fty = llvm::FunctionType::get(builder.getPtrTy(),
                                              {builder.getPtrTy()}, false);
          auto callee = context.module().getOrInsertFunction(
              "__tinyswift_string_trimmed", fty);
          setSILValue(inst.result, builder.CreateCall(callee, {str_ptr}));
        }
      } else if (name == "string_has_prefix") {
        auto* str_ptr = getSILValue(inst.operands[0]);
        auto* pfx_ptr = getSILValue(inst.operands[1]);
        if (str_ptr && pfx_ptr) {
          auto* fty = llvm::FunctionType::get(
              builder.getInt1Ty(),
              {builder.getPtrTy(), builder.getPtrTy()}, false);
          auto callee = context.module().getOrInsertFunction(
              "__tinyswift_string_has_prefix", fty);
          auto* cmp = builder.CreateCall(callee, {str_ptr, pfx_ptr});
          setSILValue(inst.result, builder.CreateZExt(cmp, builder.getInt64Ty()));
        }
      } else if (name == "string_has_suffix") {
        auto* str_ptr = getSILValue(inst.operands[0]);
        auto* sfx_ptr = getSILValue(inst.operands[1]);
        if (str_ptr && sfx_ptr) {
          auto* fty = llvm::FunctionType::get(
              builder.getInt1Ty(),
              {builder.getPtrTy(), builder.getPtrTy()}, false);
          auto callee = context.module().getOrInsertFunction(
              "__tinyswift_string_has_suffix", fty);
          auto* cmp = builder.CreateCall(callee, {str_ptr, sfx_ptr});
          setSILValue(inst.result, builder.CreateZExt(cmp, builder.getInt64Ty()));
        }
      } else if (name == "string_contains") {
        auto* str_ptr = getSILValue(inst.operands[0]);
        auto* sub_ptr = getSILValue(inst.operands[1]);
        if (str_ptr && sub_ptr) {
          auto* fty = llvm::FunctionType::get(
              builder.getInt1Ty(),
              {builder.getPtrTy(), builder.getPtrTy()}, false);
          auto callee = context.module().getOrInsertFunction(
              "__tinyswift_string_contains", fty);
          auto* cmp = builder.CreateCall(callee, {str_ptr, sub_ptr});
          setSILValue(inst.result, builder.CreateZExt(cmp, builder.getInt64Ty()));
        }

      // M65/M90: Dynamic array operations (type-erased generic).
      } else if (name == "dynarray_create") {
        // __tinyswift_dynarray_create_generic(elem_size: i64) -> ptr.
        // literal_value holds the element TypeId index.
        auto elem_type_id = SemIR::TypeId(static_cast<int32_t>(inst.literal_value));
        auto* elem_llvm_type = context.GetType(elem_type_id);
        auto& layout = context.module().getDataLayout();
        uint64_t elem_size = layout.getTypeAllocSize(elem_llvm_type);
        auto* size_val = llvm::ConstantInt::get(builder.getInt64Ty(), elem_size);
        auto* fty = llvm::FunctionType::get(
            builder.getPtrTy(), {builder.getInt64Ty()}, false);
        auto callee = context.module().getOrInsertFunction(
            "__tinyswift_dynarray_create_generic", fty);
        setSILValue(inst.result, builder.CreateCall(callee, {size_val}));
      } else if (name == "dynarray_append") {
        // __tinyswift_dynarray_append(arr: ptr, elem_ptr: ptr) -> void.
        // Alloca a temporary, store the element value, pass its address.
        auto* arr_ptr = getSILValue(inst.operands[0]);
        auto* elem_val = getSILValue(inst.operands[1]);
        if (arr_ptr && elem_val) {
          auto* elem_type = elem_val->getType();
          auto* alloca = builder.CreateAlloca(elem_type);
          builder.CreateStore(elem_val, alloca);
          auto* fty = llvm::FunctionType::get(
              builder.getVoidTy(),
              {builder.getPtrTy(), builder.getPtrTy()}, false);
          auto callee = context.module().getOrInsertFunction(
              "__tinyswift_dynarray_append", fty);
          builder.CreateCall(callee, {arr_ptr, alloca});
        } else {
          llvm::errs()
              << "WARNING: dynarray_append: null operand (arr="
              << (arr_ptr ? "ok" : "null")
              << ", elem=" << (elem_val ? "ok" : "null") << ")\n";
        }
      } else if (name == "dynarray_count") {
        // __tinyswift_dynarray_count(arr: ptr) -> i64.
        auto* arr_ptr = getSILValue(inst.operands[0]);
        if (arr_ptr) {
          auto* fty = llvm::FunctionType::get(builder.getInt64Ty(),
                                              {builder.getPtrTy()}, false);
          auto callee = context.module().getOrInsertFunction(
              "__tinyswift_dynarray_count", fty);
          setSILValue(inst.result, builder.CreateCall(callee, {arr_ptr}));
        }
      } else if (name == "dynarray_access") {
        // __tinyswift_dynarray_get(arr: ptr, idx: i64) -> ptr (to element).
        // Then load the typed element from the returned pointer.
        auto* arr_ptr = getSILValue(inst.operands[0]);
        auto* idx_val = getSILValue(inst.operands[1]);
        if (arr_ptr && idx_val) {
          if (idx_val->getType()->isIntegerTy() &&
              !idx_val->getType()->isIntegerTy(64)) {
            idx_val = builder.CreateSExt(idx_val, builder.getInt64Ty());
          }
          // Call __tinyswift_dynarray_get → returns void*.
          auto* fty = llvm::FunctionType::get(
              builder.getPtrTy(),
              {builder.getPtrTy(), builder.getInt64Ty()}, false);
          auto callee = context.module().getOrInsertFunction(
              "__tinyswift_dynarray_get", fty);
          auto* elem_ptr = builder.CreateCall(callee, {arr_ptr, idx_val});
          // Load the typed element from the returned pointer.
          auto elem_type_id = SemIR::TypeId(static_cast<int32_t>(inst.literal_value));
          auto* elem_llvm_type = context.GetType(elem_type_id);
          setSILValue(inst.result, builder.CreateLoad(elem_llvm_type, elem_ptr));
        }
      // M78: ARC operations.
      } else if (name == "alloc_class") {
        // __tinyswift_alloc(payload_size) → ptr to fields.
        // literal_value holds the class TypeId index.
        auto class_type_id = SemIR::TypeId(static_cast<int32_t>(inst.literal_value));
        auto* fields_struct = GetClassFieldsType(context, class_type_id);
        auto& layout = context.module().getDataLayout();
        uint64_t size = layout.getTypeAllocSize(fields_struct);
        auto* size_val = llvm::ConstantInt::get(builder.getInt64Ty(), size);
        auto* alloc_fty = llvm::FunctionType::get(
            builder.getPtrTy(), {builder.getInt64Ty()}, false);
        auto alloc_callee = context.module().getOrInsertFunction(
            "__tinyswift_alloc", alloc_fty);
        auto* obj_ptr = builder.CreateCall(alloc_callee, {size_val});
        // Store initial field values via GEP+store.
        for (size_t i = 0; i < inst.operand_list.size(); ++i) {
          auto* field_val = getSILValue(inst.operand_list[i]);
          if (field_val) {
            auto* gep = builder.CreateStructGEP(fields_struct, obj_ptr,
                                                static_cast<unsigned>(i));
            builder.CreateStore(field_val, gep);
          }
        }
        setSILValue(inst.result, obj_ptr);
      } else if (name == "retain") {
        // __tinyswift_retain(obj).
        auto* obj = getSILValue(inst.operands[0]);
        if (obj) {
          auto* fty = llvm::FunctionType::get(
              builder.getVoidTy(), {builder.getPtrTy()}, false);
          auto callee = context.module().getOrInsertFunction(
              "__tinyswift_retain", fty);
          builder.CreateCall(callee, {obj});
        }
      } else if (name == "release" || name == "release_cycle") {
        // __tinyswift_release(obj, deinit_fn_ptr_or_null) or
        // __tinyswift_release_cycle_candidate(obj, deinit_fn_ptr_or_null) (M97).
        auto* obj = getSILValue(inst.operands[0]);
        if (obj) {
          llvm::Value* deinit_ptr = llvm::ConstantPointerNull::get(builder.getPtrTy());
          if (inst.operands[1].is_valid()) {
            auto* fn = getSILValue(inst.operands[1]);
            if (fn) deinit_ptr = fn;
          }
          auto* fty = llvm::FunctionType::get(
              builder.getVoidTy(), {builder.getPtrTy(), builder.getPtrTy()}, false);
          const char* runtime_fn = (name == "release_cycle")
              ? "__tinyswift_release_cycle_candidate"
              : "__tinyswift_release";
          auto callee = context.module().getOrInsertFunction(runtime_fn, fty);
          builder.CreateCall(callee, {obj, deinit_ptr});
        }

      // M77: UnsafePointer operations.
      } else if (name == "unsafe_ptr_allocate") {
        // malloc(capacity * elem_size) → ptr.
        auto* capacity = getSILValue(inst.operands[0]);
        auto* elem_size = getSILValue(inst.operands[1]);
        if (capacity && elem_size) {
          auto* total = builder.CreateMul(capacity, elem_size);
          auto* fty = llvm::FunctionType::get(
              builder.getPtrTy(), {builder.getInt64Ty()}, false);
          auto callee = context.module().getOrInsertFunction("malloc", fty);
          setSILValue(inst.result, builder.CreateCall(callee, {total}));
        }
      } else if (name == "unsafe_ptr_deallocate") {
        // free(ptr).
        auto* ptr = getSILValue(inst.operands[0]);
        if (ptr) {
          auto* fty = llvm::FunctionType::get(
              builder.getVoidTy(), {builder.getPtrTy()}, false);
          auto callee = context.module().getOrInsertFunction("free", fty);
          builder.CreateCall(callee, {ptr});
        }
      } else if (name == "unsafe_ptr_subscript") {
        // GEP(i64, ptr, idx) + load.
        auto* ptr = getSILValue(inst.operands[0]);
        auto* idx = getSILValue(inst.operands[1]);
        if (ptr && idx) {
          auto* gep = builder.CreateGEP(builder.getInt64Ty(), ptr, {idx});
          setSILValue(inst.result, builder.CreateLoad(builder.getInt64Ty(), gep));
        }
      } else if (name == "unsafe_ptr_subscript_addr") {
        // GEP(i64, ptr, idx) → address for store.
        auto* ptr = getSILValue(inst.operands[0]);
        auto* idx = getSILValue(inst.operands[1]);
        if (ptr && idx) {
          setSILValue(inst.result, builder.CreateGEP(builder.getInt64Ty(), ptr, {idx}));
        }
      // M81: Error handling runtime calls.
      } else if (name == "error_set") {
        auto* fn_type = llvm::FunctionType::get(
            builder.getVoidTy(), {builder.getInt64Ty()}, false);
        auto callee = context.module().getOrInsertFunction(
            "__tinyswift_error_set", fn_type);
        llvm::Value* val = nullptr;
        if (!inst.operand_list.empty()) {
          val = getSILValue(inst.operand_list[0]);
        }
        if (!val) {
          // Use literal_value as fallback.
          val = builder.getInt64(inst.literal_value);
        }
        // Widen to i64 if needed (e.g. enum tag is i8).
        if (!val->getType()->isIntegerTy(64)) {
          val = builder.CreateSExt(val, builder.getInt64Ty());
        }
        builder.CreateCall(callee, {val});
      } else if (name == "error_check") {
        auto* fn_type = llvm::FunctionType::get(
            builder.getInt32Ty(), {}, false);
        auto callee = context.module().getOrInsertFunction(
            "__tinyswift_error_check", fn_type);
        auto* result_i32 = builder.CreateCall(callee, {});
        // Truncate i32 to i1 for branch condition.
        auto* result_i1 = builder.CreateICmpNE(
            result_i32, builder.getInt32(0));
        setSILValue(inst.result, result_i1);
      } else if (name == "error_clear") {
        auto* fn_type = llvm::FunctionType::get(
            builder.getVoidTy(), {}, false);
        auto callee = context.module().getOrInsertFunction(
            "__tinyswift_error_clear", fn_type);
        builder.CreateCall(callee, {});
      } else if (name == "error_get") {
        auto* fn_type = llvm::FunctionType::get(
            builder.getInt64Ty(), {}, false);
        auto callee = context.module().getOrInsertFunction(
            "__tinyswift_error_get", fn_type);
        setSILValue(inst.result, builder.CreateCall(callee, {}));
      } else {
        llvm::errs() << "WARNING: unrecognized builtin in LowerSILInst: "
                     << name << "\n";
      }
      break;
    }

    case TinySIL::SILInstKind::StructInst: {
      auto* struct_ty = GetLLVMTypeForSIL(context, inst.result.type);
      if (struct_ty->isVoidTy() || !struct_ty->isStructTy()) {
        setSILValue(inst.result,
                    llvm::UndefValue::get(builder.getInt64Ty()));
        break;
      }
      llvm::Value* agg = llvm::UndefValue::get(struct_ty);
      for (size_t i = 0; i < inst.operand_list.size(); ++i) {
        auto* field = getSILValue(inst.operand_list[i]);
        if (field) {
          agg = builder.CreateInsertValue(agg, field, {static_cast<unsigned>(i)});
        }
      }
      setSILValue(inst.result, agg);
      break;
    }

    case TinySIL::SILInstKind::StructExtract: {
      auto* base = getSILValue(inst.operands[0]);
      if (base && inst.element_index >= 0) {
        // M78: If the base is a pointer (class instance), use GEP+load.
        if (base->getType()->isPointerTy()) {
          auto base_type_id = SemIR::TypeId(inst.operands[0].type.type_index);
          auto* fields_struct = GetClassFieldsType(context, base_type_id);
          auto* gep = builder.CreateStructGEP(
              fields_struct, base,
              static_cast<unsigned>(inst.element_index));
          auto* result_ty = GetLLVMTypeForSIL(context, inst.result.type);
          auto* loaded = builder.CreateLoad(result_ty, gep);
          setSILValue(inst.result, loaded);
        } else {
          auto* val = builder.CreateExtractValue(
              base, {static_cast<unsigned>(inst.element_index)});
          setSILValue(inst.result, val);
        }
      }
      break;
    }

    case TinySIL::SILInstKind::StructElementAddr: {
      // Compute a pointer to a struct field (GEP). The operand is a pointer
      // to the struct (from AllocStack); the result is a pointer to the field.
      auto* base_ptr = getSILValue(inst.operands[0]);
      if (base_ptr && inst.element_index >= 0) {
        auto struct_sil_type = inst.operands[0].type.getObjectType();
        auto base_type_id = SemIR::TypeId(struct_sil_type.type_index);
        // M78: For class types, use fields struct type for GEP.
        auto base_type_inst = context.sem_ir().types().GetAsInst(base_type_id);
        llvm::Type* struct_llvm_type;
        if (base_type_inst.Is<SemIR::ClassType>()) {
          struct_llvm_type = GetClassFieldsType(context, base_type_id);
        } else {
          struct_llvm_type = GetLLVMTypeForSIL(context, struct_sil_type);
        }
        auto* gep = builder.CreateStructGEP(
            struct_llvm_type, base_ptr,
            static_cast<unsigned>(inst.element_index));
        setSILValue(inst.result, gep);
      }
      break;
    }

    case TinySIL::SILInstKind::TupleInst: {
      if (inst.operand_list.empty()) {
        // Empty tuple () — used for void returns.
        setSILValue(inst.result,
                    llvm::UndefValue::get(builder.getVoidTy()));
        break;
      }
      // Use the result's declared SIL type to get the correct LLVM struct type.
      // This ensures named struct types (e.g. enum payloads) are preserved
      // rather than creating a new anonymous struct from the elements.
      llvm::SmallVector<llvm::Value*> elem_vals;
      for (const auto& elem : inst.operand_list) {
        auto* val = getSILValue(elem);
        if (val) elem_vals.push_back(val);
      }
      auto* declared_ty = GetLLVMTypeForSIL(context, inst.result.type);
      llvm::StructType* tuple_ty = nullptr;
      if (declared_ty && declared_ty->isStructTy()) {
        tuple_ty = llvm::cast<llvm::StructType>(declared_ty);
      } else {
        // Fallback: infer from element types.
        llvm::SmallVector<llvm::Type*> elem_types;
        for (auto* v : elem_vals) elem_types.push_back(v->getType());
        tuple_ty = llvm::StructType::get(context.llvm_context(), elem_types);
      }
      llvm::Value* agg = llvm::UndefValue::get(tuple_ty);
      for (size_t i = 0; i < elem_vals.size(); ++i) {
        agg = builder.CreateInsertValue(agg, elem_vals[i],
                                        {static_cast<unsigned>(i)});
      }
      setSILValue(inst.result, agg);
      break;
    }

    case TinySIL::SILInstKind::TupleExtract: {
      auto* base = getSILValue(inst.operands[0]);
      if (base && inst.element_index >= 0) {
        auto* val = builder.CreateExtractValue(
            base, {static_cast<unsigned>(inst.element_index)});
        setSILValue(inst.result, val);
      }
      break;
    }

    case TinySIL::SILInstKind::EnumInst: {
      // Enum case without payload: wrap discriminant into { i64, ... } struct.
      // For mixed enums (some cases have payload), the struct has 2+ fields;
      // fill the payload slot(s) with zero.
      auto* tag_val = llvm::ConstantInt::get(builder.getInt64Ty(),
                                              inst.literal_value);
      auto* llvm_ty = GetLLVMTypeForSIL(context, inst.result.type);
      if (llvm_ty && llvm_ty->isStructTy()) {
        auto* st = llvm::cast<llvm::StructType>(llvm_ty);
        llvm::SmallVector<llvm::Constant*> vals;
        vals.push_back(tag_val);
        // Fill remaining fields with zeroes (mixed-enum payload slots).
        for (unsigned i = 1; i < st->getNumElements(); ++i) {
          vals.push_back(llvm::Constant::getNullValue(st->getElementType(i)));
        }
        auto* agg = llvm::ConstantStruct::get(st, vals);
        setSILValue(inst.result, agg);
      } else {
        // Fallback: just the i64 (should not happen for well-typed IR).
        setSILValue(inst.result, tag_val);
      }
      break;
    }

    // Ownership instructions are no-ops for now.
    case TinySIL::SILInstKind::CopyValue:
    case TinySIL::SILInstKind::BeginBorrow: {
      auto* val = getSILValue(inst.operands[0]);
      if (val) setSILValue(inst.result, val);
      break;
    }

    case TinySIL::SILInstKind::DestroyValue:
    case TinySIL::SILInstKind::EndBorrow:
      // No-op.
      break;

    case TinySIL::SILInstKind::DebugValue:
      // No-op for now.
      break;

    default:
      // Unknown SIL instruction kind — fatal error to catch missing handlers.
      llvm::errs() << "FATAL: unhandled SIL instruction kind in LowerSILInst: "
                   << static_cast<int>(inst.kind) << "\n";
      llvm::report_fatal_error(
          "unhandled SIL instruction kind in LowerSILInst");
  }
}

// Lowers a SIL function body.
auto LowerSILFunctionBody(Context& context,
                          const TinySIL::SILFunction& sil_fn,
                          llvm::Function* llvm_fn) -> void {
  if (sil_fn.is_declaration || sil_fn.blocks.empty()) {
    return;
  }

  // M119: Set debug scope to the function's DISubprogram.
  if (context.debug_enabled() && llvm_fn->getSubprogram()) {
    context.SetCurrentScope(llvm_fn->getSubprogram());
  }

  llvm::DenseMap<int32_t, llvm::Value*> sil_values;
  llvm::DenseMap<int32_t, llvm::BasicBlock*> sil_blocks;
  // M53: Maps partial_apply result SIL value IDs to their captured LLVM values.
  llvm::DenseMap<int32_t, llvm::SmallVector<llvm::Value*>> closure_captures;

  // Create all basic blocks.
  for (const auto& bb : sil_fn.blocks) {
    auto* llvm_bb = llvm::BasicBlock::Create(
        context.llvm_context(), "bb" + std::to_string(bb->id), llvm_fn);
    sil_blocks[bb->id] = llvm_bb;
  }

  // Map entry block arguments to function parameters.
  auto* entry_bb = sil_fn.getEntryBlock();
  if (entry_bb) {
    unsigned arg_idx = 0;
    for (const auto& arg : entry_bb->args) {
      if (arg.is_valid() && arg_idx < llvm_fn->arg_size()) {
        sil_values[arg.id] = llvm_fn->getArg(arg_idx);
        ++arg_idx;
      }
    }
  }

  // Lower each block.
  for (const auto& bb : sil_fn.blocks) {
    auto* llvm_bb = sil_blocks[bb->id];
    context.builder().SetInsertPoint(llvm_bb);

    for (const auto& inst : bb->insts) {
      // M119: Set debug location from SIL instruction's loc_id.
      if (context.debug_enabled() && inst->loc_id.has_value()) {
        context.SetDebugLoc(inst->loc_id);
      }
      LowerSILInst(context, *inst, sil_values, sil_blocks, llvm_fn,
                   closure_captures);
    }

    // If block has no terminator, add a fallback.
    if (llvm_bb->getTerminator() == nullptr) {
      if (llvm_fn->getReturnType()->isVoidTy()) {
        context.builder().CreateRetVoid();
      } else {
        context.builder().CreateUnreachable();
      }
    }
  }
}

}  // namespace

auto LowerSILToLLVM(llvm::LLVMContext& llvm_context,
                    llvm::StringRef module_name,
                    const TinySIL::SILModule& sil_module,
                    const SemIR::File& sem_ir,
                    const LowerToLLVMOptions& options,
                    const Lex::TokenizedBuffer* tokens)
    -> std::unique_ptr<llvm::Module> {
  auto module = std::make_unique<llvm::Module>(module_name, llvm_context);

  Context context(*module, llvm_context, sem_ir, tokens,
                  options.want_debug_info);

  // Forward-declare all SIL functions.
  llvm::DenseMap<llvm::StringRef, llvm::Function*> fn_map;
  for (const auto& sil_fn : sil_module.functions) {
    auto* fn_type = BuildSILFunctionType(context, *sil_fn);
    auto* llvm_fn = llvm::Function::Create(
        fn_type, llvm::Function::ExternalLinkage, sil_fn->name,
        &context.module());

    // M119: Create DISubprogram for each SIL function.
    if (context.debug_enabled()) {
      unsigned line = 0;
      // Use the SemIR function decl location if available.
      if (sil_fn->sem_ir_function_id >= 0) {
        auto func_id = SemIR::FunctionId(sil_fn->sem_ir_function_id);
        auto& function = sem_ir.functions().Get(func_id);
        if (!function.body_block_ids.empty()) {
          auto first_block =
              sem_ir.inst_blocks().Get(function.body_block_ids[0]);
          if (!first_block.empty()) {
            unsigned col = 0;
            auto loc_id = sem_ir.insts().GetCanonicalLocId(first_block[0]);
            context.ResolveLocToLineCol(loc_id, line, col);
          }
        }
      }
      auto* sp = context.di_builder()->createFunction(
          context.di_file(), sil_fn->name, sil_fn->name,
          context.di_file(), line,
          context.CreateFunctionDIType(), line,
          llvm::DINode::FlagPrototyped,
          llvm::DISubprogram::SPFlagDefinition);
      llvm_fn->setSubprogram(sp);
    }

    fn_map[sil_fn->name] = llvm_fn;
  }

  // Lower all function bodies.
  for (const auto& sil_fn : sil_module.functions) {
    auto it = fn_map.find(sil_fn->name);
    if (it != fn_map.end()) {
      LowerSILFunctionBody(context, *sil_fn, it->second);
    }
  }

  // M119: Finalize debug info.
  if (context.debug_enabled()) {
    context.di_builder()->finalize();
  }

  if (options.vlog_stream) {
    TINYSWIFT_VLOG_TO(options.vlog_stream, "*** llvm::Module (from SIL) ***\n");
    module->print(*options.vlog_stream, /*AAW=*/nullptr,
                  /*ShouldPreserveUseListOrder=*/false,
                  /*IsForDebug=*/true);
  }

  if (options.llvm_verifier_stream) {
    TINYSWIFT_CHECK(!llvm::verifyModule(*module, options.llvm_verifier_stream));
  }

  return module;
}

}  // namespace TinySwift::Lower
