// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "toolchain/lower/context.h"

#include "common/check.h"
#include "llvm/BinaryFormat/Dwarf.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/Path.h"
#include "toolchain/parse/tree.h"
#include "toolchain/sem_ir/typed_insts.h"

namespace TinySwift::Lower {

Context::Context(llvm::Module& module, llvm::LLVMContext& llvm_context,
                 const SemIR::File& sem_ir,
                 const Lex::TokenizedBuffer* tokens,
                 bool want_debug_info)
    : module_(module),
      llvm_context_(llvm_context),
      sem_ir_(sem_ir),
      builder_(llvm_context),
      tokens_(tokens) {
  if (want_debug_info && tokens_) {
    debug_enabled_ = true;
    di_builder_ = std::make_unique<llvm::DIBuilder>(module);

    // DWARF v4 for broadest debugger support.
    module.addModuleFlag(llvm::Module::Warning, "Dwarf Version", 4);
    module.addModuleFlag(llvm::Module::Warning, "Debug Info Version",
                         llvm::DEBUG_METADATA_VERSION);

    // Create DIFile from the source filename.
    llvm::StringRef filename = sem_ir.filename();
    llvm::SmallString<256> abs_path(filename);
    llvm::StringRef dir = llvm::sys::path::parent_path(abs_path);
    llvm::StringRef file = llvm::sys::path::filename(abs_path);
    if (dir.empty()) dir = ".";

    di_file_ = di_builder_->createFile(file, dir);
    di_compile_unit_ = di_builder_->createCompileUnit(
        llvm::dwarf::DW_LANG_C, di_file_, "TinySwift",
        /*isOptimized=*/false, /*Flags=*/"", /*RV=*/0);
    current_scope_ = di_compile_unit_;
  }
}

auto Context::ResolveLocToLineCol(SemIR::LocId loc_id,
                                  unsigned& line, unsigned& col) -> bool {
  if (!debug_enabled_ || !tokens_ || !loc_id.has_value()) {
    return false;
  }

  // Canonicalize: resolve InstId-backed LocIds to NodeId-backed.
  auto canonical = sem_ir_.insts().GetCanonicalLocId(loc_id);
  if (canonical.kind() != SemIR::LocId::Kind::NodeId) {
    return false;
  }

  auto node_id = canonical.node_id();
  if (!node_id.has_value()) {
    return false;
  }

  // Multi-file safety: LocIds from non-primary files have NodeIds that are
  // invalid for the primary file's parse tree. Bounds-check to avoid crash.
  if (node_id.index >= sem_ir_.parse_tree().size()) {
    return false;
  }

  auto token = sem_ir_.parse_tree().node_token(node_id);
  line = static_cast<unsigned>(tokens_->GetLineNumber(token));
  col = static_cast<unsigned>(tokens_->GetColumnNumber(token));
  return true;
}

auto Context::SetDebugLoc(SemIR::LocId loc_id) -> void {
  if (!debug_enabled_ || !current_scope_) return;

  unsigned line = 0, col = 0;
  // Try to resolve the source location. If it fails (e.g. NodeId from a
  // non-primary file), use line 0 as a fallback to satisfy LLVM's requirement
  // that all instructions in a function with debug info have a !dbg location.
  ResolveLocToLineCol(loc_id, line, col);
  auto* di_loc =
      llvm::DILocation::get(llvm_context_, line, col, current_scope_);
  builder_.SetCurrentDebugLocation(di_loc);
}

auto Context::ClearDebugLoc() -> void {
  if (!debug_enabled_) return;
  builder_.SetCurrentDebugLocation(llvm::DebugLoc());
}

auto Context::CreateFunctionDIType() -> llvm::DISubroutineType* {
  // Minimal: unspecified types for now. M120 will refine with real types.
  llvm::SmallVector<llvm::Metadata*> param_types;
  param_types.push_back(nullptr);  // Return type (unspecified).
  return di_builder_->createSubroutineType(
      di_builder_->getOrCreateTypeArray(param_types));
}

auto Context::GetOrCreateDIType(SemIR::TypeId type_id) -> llvm::DIType* {
  if (!debug_enabled_ || !type_id.has_value()) {
    return nullptr;
  }

  auto it = di_type_cache_.find(type_id.index);
  if (it != di_type_cache_.end()) {
    return it->second;
  }

  auto inst = sem_ir_.types().GetAsInst(type_id);
  llvm::DIType* result = nullptr;

  switch (inst.kind()) {
    case SemIR::InstKind::IntType:
    case SemIR::InstKind::IntLiteralType:
      result = di_builder_->createBasicType("Int", 64,
                                            llvm::dwarf::DW_ATE_signed);
      break;
    case SemIR::InstKind::BoolType:
      result = di_builder_->createBasicType("Bool", 8,
                                            llvm::dwarf::DW_ATE_boolean);
      break;
    case SemIR::InstKind::FloatType:
      result = di_builder_->createBasicType("Float", 32,
                                            llvm::dwarf::DW_ATE_float);
      break;
    case SemIR::InstKind::DoubleType:
      result = di_builder_->createBasicType("Double", 64,
                                            llvm::dwarf::DW_ATE_float);
      break;
    case SemIR::InstKind::StringType:
      result = di_builder_->createBasicType("String", 64,
                                            llvm::dwarf::DW_ATE_address);
      break;
    case SemIR::InstKind::PointerType:
    case SemIR::InstKind::FunctionType:
      result = di_builder_->createPointerType(nullptr, 64);
      break;
    case SemIR::InstKind::OptionalType: {
      // M121: Model as {Bool, T} struct.
      auto optional = inst.As<SemIR::OptionalType>();
      auto inner_type_id =
          sem_ir_.types().GetTypeIdForTypeInstId(optional.inner_type_id);
      auto* inner_di = GetOrCreateDIType(inner_type_id);
      auto* bool_di = di_builder_->createBasicType("Bool", 8,
                                                    llvm::dwarf::DW_ATE_boolean);

      llvm::SmallVector<llvm::Metadata*> elements;
      elements.push_back(di_builder_->createMemberType(
          di_compile_unit_, "has_value", di_file_, 0, 8, 8, 0,
          llvm::DINode::FlagZero, bool_di));

      uint64_t inner_size = inner_di ? inner_di->getSizeInBits() : 64;
      uint64_t inner_align = inner_size > 0 ? inner_size : 64;
      elements.push_back(di_builder_->createMemberType(
          di_compile_unit_, "value", di_file_, 0, inner_size, inner_align, 64,
          llvm::DINode::FlagZero, inner_di ? inner_di : di_builder_->createBasicType("Int", 64, llvm::dwarf::DW_ATE_signed)));

      result = di_builder_->createStructType(
          di_compile_unit_, "Optional", di_file_, 0,
          64 + inner_size, 64,
          llvm::DINode::FlagZero, nullptr,
          di_builder_->getOrCreateArray(elements));
      break;
    }
    case SemIR::InstKind::StructType: {
      // M121: Create DICompositeType for struct.
      auto struct_type = inst.As<SemIR::StructType>();
      auto& scope = sem_ir_.name_scopes().Get(struct_type.name_scope_id);
      std::string name;
      if (scope.name_id.AsIdentifierId().has_value()) {
        name = std::string(
            sem_ir_.identifiers().Get(scope.name_id.AsIdentifierId()));
      }

      // Collect fields sorted by index.
      struct FieldEntry {
        int32_t idx;
        SemIR::TypeId type_id;
        std::string name;
      };
      llvm::SmallVector<FieldEntry> fields;
      for (const auto& kv : scope.names) {
        auto fi = sem_ir_.insts().Get(kv.second);
        if (auto sf = fi.TryAs<SemIR::StructField>()) {
          std::string field_name;
          auto name_id = SemIR::NameId(kv.first);
          if (name_id.AsIdentifierId().has_value()) {
            field_name = std::string(
                sem_ir_.identifiers().Get(name_id.AsIdentifierId()));
          }
          fields.push_back({sf->index.index, sf->type_id, field_name});
        }
      }
      std::sort(fields.begin(), fields.end(),
                [](const FieldEntry& a, const FieldEntry& b) {
                  return a.idx < b.idx;
                });

      llvm::SmallVector<llvm::Metadata*> elements;
      uint64_t offset = 0;
      for (const auto& fe : fields) {
        auto* field_di = GetOrCreateDIType(fe.type_id);
        uint64_t field_size = field_di ? field_di->getSizeInBits() : 64;
        uint64_t field_align = field_size > 0 ? field_size : 64;
        elements.push_back(di_builder_->createMemberType(
            di_compile_unit_, fe.name, di_file_, 0, field_size, field_align,
            offset, llvm::DINode::FlagZero,
            field_di ? field_di : di_builder_->createBasicType("Int", 64, llvm::dwarf::DW_ATE_signed)));
        offset += field_size;
      }

      result = di_builder_->createStructType(
          di_compile_unit_, name, di_file_, 0,
          offset, offset > 0 ? 64 : 0,
          llvm::DINode::FlagZero, nullptr,
          di_builder_->getOrCreateArray(elements));
      break;
    }
    case SemIR::InstKind::ClassType: {
      // M121: Class is a pointer to a struct with refcount + fields.
      result = di_builder_->createPointerType(
          di_builder_->createBasicType("ClassObj", 64, llvm::dwarf::DW_ATE_address),
          64);
      break;
    }
    case SemIR::InstKind::TupleType: {
      // M121: Model as anonymous struct with .0, .1, etc.
      auto tuple_type = inst.As<SemIR::TupleType>();
      llvm::SmallVector<llvm::Metadata*> elements;
      uint64_t offset = 0;
      int elem_idx = 0;
      if (tuple_type.element_types_id.has_value() &&
          tuple_type.element_types_id != SemIR::InstBlockId::Empty) {
        auto elem_inst_ids =
            sem_ir_.inst_blocks().Get(tuple_type.element_types_id);
        for (auto elem_inst_id : elem_inst_ids) {
          auto elem_type_id = sem_ir_.types().GetTypeIdForTypeInstId(
              SemIR::TypeInstId::UnsafeMake(elem_inst_id));
          auto* elem_di = GetOrCreateDIType(elem_type_id);
          uint64_t elem_size = elem_di ? elem_di->getSizeInBits() : 64;
          uint64_t elem_align = elem_size > 0 ? elem_size : 64;
          std::string elem_name = "." + std::to_string(elem_idx);
          elements.push_back(di_builder_->createMemberType(
              di_compile_unit_, elem_name, di_file_, 0, elem_size, elem_align,
              offset, llvm::DINode::FlagZero,
              elem_di ? elem_di : di_builder_->createBasicType("Int", 64, llvm::dwarf::DW_ATE_signed)));
          offset += elem_size;
          ++elem_idx;
        }
      }

      result = di_builder_->createStructType(
          di_compile_unit_, "", di_file_, 0,
          offset, offset > 0 ? 64 : 0,
          llvm::DINode::FlagZero, nullptr,
          di_builder_->getOrCreateArray(elements));
      break;
    }
    case SemIR::InstKind::EnumDecl: {
      // M121: Simple enum as DW_TAG_enumeration_type.
      result = di_builder_->createBasicType("Enum", 64,
                                            llvm::dwarf::DW_ATE_signed);
      break;
    }
    default:
      // Fall back to an unspecified type.
      result = di_builder_->createBasicType("_", 64, llvm::dwarf::DW_ATE_signed);
      break;
  }

  di_type_cache_[type_id.index] = result;
  return result;
}

auto Context::GetType(SemIR::TypeId type_id) -> llvm::Type* {
  auto it = type_cache_.find(type_id.index);
  if (it != type_cache_.end()) {
    return it->second;
  }
  auto* result = LowerType(*this, type_id);
  type_cache_[type_id.index] = result;
  return result;
}

auto Context::SetLocal(SemIR::InstId inst_id, llvm::Value* value) -> void {
  auto result = locals_.insert({inst_id.index, value});
  TINYSWIFT_CHECK(result.second || result.first->second == value,
               "Duplicate local for inst {0}", inst_id);
}

auto Context::GetLocal(SemIR::InstId inst_id) -> llvm::Value* {
  auto it = locals_.find(inst_id.index);
  TINYSWIFT_CHECK(it != locals_.end(), "Missing local for inst {0}", inst_id);
  return it->second;
}

auto Context::TryGetLocal(SemIR::InstId inst_id) -> llvm::Value* {
  auto it = locals_.find(inst_id.index);
  if (it != locals_.end()) {
    return it->second;
  }
  return nullptr;
}

auto Context::GetFunction(SemIR::FunctionId function_id) -> llvm::Function* {
  auto it = functions_.find(function_id.index);
  TINYSWIFT_CHECK(it != functions_.end(), "Missing function for {0}",
               function_id);
  return it->second;
}

auto Context::SetFunction(SemIR::FunctionId function_id,
                          llvm::Function* function) -> void {
  functions_[function_id.index] = function;
}

auto Context::GetBlock(SemIR::InstBlockId block_id) -> llvm::BasicBlock* {
  auto it = blocks_.find(block_id.index);
  TINYSWIFT_CHECK(it != blocks_.end(), "Missing block for {0}", block_id);
  return it->second;
}

auto Context::GetOrCreateBlock(SemIR::InstBlockId block_id,
                               llvm::Function* function) -> llvm::BasicBlock* {
  auto it = blocks_.find(block_id.index);
  if (it != blocks_.end()) {
    return it->second;
  }
  auto* block = llvm::BasicBlock::Create(llvm_context_, "", function);
  blocks_[block_id.index] = block;
  return block;
}

auto Context::SetBlockArg(SemIR::InstBlockId block_id,
                          llvm::Value* value) -> void {
  block_args_[block_id.index] = value;
}

auto Context::GetBlockArg(SemIR::InstBlockId block_id) -> llvm::Value* {
  auto it = block_args_.find(block_id.index);
  TINYSWIFT_CHECK(it != block_args_.end(), "Missing block arg for {0}",
               block_id);
  return it->second;
}

}  // namespace TinySwift::Lower
