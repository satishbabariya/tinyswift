// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Type.h"
#include "toolchain/lower/context.h"
#include "toolchain/sem_ir/typed_insts.h"

namespace TinySwift::Lower {

auto LowerType(Context& context, SemIR::TypeId type_id) -> llvm::Type* {
  auto& sem_ir = context.sem_ir();
  auto inst = sem_ir.types().GetAsInst(type_id);

  switch (inst.kind()) {
    case SemIR::InstKind::BoolType:
      return llvm::Type::getInt1Ty(context.llvm_context());

    case SemIR::InstKind::IntType: {
      // Default to 64-bit integers.
      return llvm::Type::getInt64Ty(context.llvm_context());
    }

    case SemIR::InstKind::IntLiteralType:
      // Integer literals default to i64.
      return llvm::Type::getInt64Ty(context.llvm_context());

    case SemIR::InstKind::FloatType:
      return llvm::Type::getFloatTy(context.llvm_context());

    case SemIR::InstKind::DoubleType:
      return llvm::Type::getDoubleTy(context.llvm_context());

    case SemIR::InstKind::StringType:
      // Strings are opaque pointers.
      return llvm::PointerType::get(context.llvm_context(), 0);

    case SemIR::InstKind::PointerType:
      return llvm::PointerType::get(context.llvm_context(), 0);

    case SemIR::InstKind::FunctionType:
      // Function types are represented as opaque pointers at the value level.
      return llvm::PointerType::get(context.llvm_context(), 0);

    case SemIR::InstKind::OptionalType: {
      auto optional = inst.As<SemIR::OptionalType>();
      auto inner_type_id =
          sem_ir.types().GetTypeIdForTypeInstId(optional.inner_type_id);
      auto* inner_llvm_type = context.GetType(inner_type_id);
      // Optional is {i1, T} - has_value flag + payload.
      return llvm::StructType::get(
          context.llvm_context(),
          {llvm::Type::getInt1Ty(context.llvm_context()), inner_llvm_type});
    }

    case SemIR::InstKind::StructType: {
      auto struct_type = inst.As<SemIR::StructType>();
      auto& scope = sem_ir.name_scopes().Get(struct_type.name_scope_id);
      llvm::StringRef name = "";
      if (scope.name_id.AsIdentifierId().has_value()) {
        name = sem_ir.identifiers().Get(scope.name_id.AsIdentifierId());
      }

      // Look for StructField instructions to build the body.
      // Search all instructions for StructField with matching type.
      llvm::SmallVector<llvm::Type*> field_types;
      auto type_inst_id = sem_ir.types().GetTypeInstId(type_id);
      for (auto [iter_id, iter_inst] : sem_ir.insts().enumerate()) {
        if (auto field = iter_inst.TryAs<SemIR::StructField>()) {
          // Check if this field belongs to this struct by matching its type_id.
          if (field->type_id.has_value() && field->type_id != SemIR::ErrorInst::TypeId) {
            // The StructField's type_id is the field's type, not the struct's type.
            // We need a different approach: collect fields registered in the scope.
          }
        }
      }

      // For now, build the struct from NameScope entries.
      auto* llvm_struct = llvm::StructType::create(context.llvm_context(), name);

      // Collect field types from the scope's name entries.
      llvm::SmallVector<llvm::Type*> body_types;
      for (auto& [name_id_idx, inst_id] : scope.names) {
        auto field_inst = sem_ir.insts().Get(inst_id);
        auto field_type_id = field_inst.type_id();
        if (field_type_id.has_value() &&
            field_type_id != SemIR::ErrorInst::TypeId &&
            field_type_id != SemIR::TypeType::TypeId) {
          auto* ft = context.GetType(field_type_id);
          if (!ft->isVoidTy()) {
            body_types.push_back(ft);
          }
        }
      }
      if (!body_types.empty()) {
        llvm_struct->setBody(body_types);
      }
      return llvm_struct;
    }

    case SemIR::InstKind::ClassType: {
      auto class_type = inst.As<SemIR::ClassType>();
      auto& scope = sem_ir.name_scopes().Get(class_type.name_scope_id);
      llvm::StringRef name = "";
      if (scope.name_id.AsIdentifierId().has_value()) {
        name = sem_ir.identifiers().Get(scope.name_id.AsIdentifierId());
      }
      auto* llvm_struct = llvm::StructType::create(context.llvm_context(), name);

      // Collect field types from scope.
      llvm::SmallVector<llvm::Type*> body_types;
      for (auto& [name_id_idx, inst_id] : scope.names) {
        auto field_inst = sem_ir.insts().Get(inst_id);
        auto field_type_id = field_inst.type_id();
        if (field_type_id.has_value() &&
            field_type_id != SemIR::ErrorInst::TypeId &&
            field_type_id != SemIR::TypeType::TypeId) {
          auto* ft = context.GetType(field_type_id);
          if (!ft->isVoidTy()) {
            body_types.push_back(ft);
          }
        }
      }
      if (!body_types.empty()) {
        llvm_struct->setBody(body_types);
      }
      return llvm_struct;
    }

    case SemIR::InstKind::EnumType: {
      auto enum_type = inst.As<SemIR::EnumType>();
      auto& scope = sem_ir.name_scopes().Get(enum_type.name_scope_id);
      llvm::StringRef name = "";
      if (scope.name_id.AsIdentifierId().has_value()) {
        name = sem_ir.identifiers().Get(scope.name_id.AsIdentifierId());
      }
      // Tagged union: { i64 discriminant, [payload_size x i8] }
      // For simple enums (no payload), just use { i64 }.
      auto* tag_type = llvm::Type::getInt64Ty(context.llvm_context());
      auto* llvm_struct = llvm::StructType::create(
          context.llvm_context(), {tag_type}, name);
      return llvm_struct;
    }

    case SemIR::InstKind::TupleType: {
      auto tuple_type = inst.As<SemIR::TupleType>();
      llvm::SmallVector<llvm::Type*> elem_types;
      if (tuple_type.element_types_id.has_value() &&
          tuple_type.element_types_id != SemIR::InstBlockId::Empty) {
        auto elem_inst_ids =
            sem_ir.inst_blocks().Get(tuple_type.element_types_id);
        for (auto elem_inst_id : elem_inst_ids) {
          auto elem_inst = sem_ir.insts().Get(elem_inst_id);
          auto elem_type_id = sem_ir.types().GetTypeIdForTypeInstId(
              SemIR::TypeInstId::UnsafeMake(elem_inst_id));
          elem_types.push_back(context.GetType(elem_type_id));
        }
      }
      return llvm::StructType::get(context.llvm_context(), elem_types);
    }

    case SemIR::InstKind::TypeType:
    case SemIR::InstKind::NamespaceType:
      // Type and namespace are compile-time only, no runtime representation.
      return llvm::Type::getVoidTy(context.llvm_context());

    default:
      // For any type we don't specifically handle, fall back to void.
      return llvm::Type::getVoidTy(context.llvm_context());
  }
}

}  // namespace TinySwift::Lower
