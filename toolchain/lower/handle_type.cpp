// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <algorithm>

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

      auto* llvm_struct = llvm::StructType::create(context.llvm_context(), name);

      // Collect StructField entries from the scope, sort by field index.
      struct FieldEntry {
        int32_t idx;
        SemIR::TypeId type_id;
      };
      llvm::SmallVector<FieldEntry> fields;
      for (const auto& kv : scope.names) {
        auto fi = sem_ir.insts().Get(kv.second);
        if (auto sf = fi.TryAs<SemIR::StructField>()) {
          fields.push_back({sf->index.index, sf->type_id});
        }
      }
      std::sort(fields.begin(), fields.end(),
                [](const FieldEntry& a, const FieldEntry& b) {
                  return a.idx < b.idx;
                });
      llvm::SmallVector<llvm::Type*> field_types;
      for (const auto& fe : fields) {
        field_types.push_back(context.GetType(fe.type_id));
      }
      // Always set body (even empty) to avoid opaque struct — opaque structs
      // cannot be passed by value as function arguments in LLVM 20+.
      llvm_struct->setBody(field_types);
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

      // Collect StructField entries (own + inherited) sorted by field index.
      struct FieldEntry {
        int32_t idx;
        SemIR::TypeId type_id;
      };
      llvm::SmallVector<FieldEntry> fields;
      for (const auto& kv : scope.names) {
        auto fi = sem_ir.insts().Get(kv.second);
        if (auto sf = fi.TryAs<SemIR::StructField>()) {
          fields.push_back({sf->index.index, sf->type_id});
        }
      }
      std::sort(fields.begin(), fields.end(),
                [](const FieldEntry& a, const FieldEntry& b) {
                  return a.idx < b.idx;
                });
      llvm::SmallVector<llvm::Type*> field_types;
      for (const auto& fe : fields) {
        field_types.push_back(context.GetType(fe.type_id));
      }
      // Always set body (even empty) to avoid opaque struct.
      llvm_struct->setBody(field_types);
      return llvm_struct;
    }

    case SemIR::InstKind::EnumDecl: {
      auto enum_type = inst.As<SemIR::EnumDecl>();
      auto& scope = sem_ir.name_scopes().Get(enum_type.name_scope_id);
      llvm::StringRef name = "";
      if (scope.name_id.AsIdentifierId().has_value()) {
        name = sem_ir.identifiers().Get(scope.name_id.AsIdentifierId());
      }
      auto* tag_type = llvm::Type::getInt64Ty(context.llvm_context());

      // Check if any case has an associated payload. If so, use
      // { i64 tag, payload_type } layout; otherwise just { i64 }.
      llvm::Type* payload_type = nullptr;
      for (const auto& kv : scope.names) {
        auto case_inst = sem_ir.insts().Get(kv.second);
        if (auto ecwp = case_inst.TryAs<SemIR::EnumCaseWithPayload>()) {
          // Get the payload LLVM type from its TypeInstId.
          auto pt_id = sem_ir.types().GetTypeIdForTypeInstId(
              SemIR::TypeInstId::UnsafeMake(ecwp->payload_type_id));
          payload_type = context.GetType(pt_id);
          break;  // All payload cases share the same LLVM layout.
        }
      }

      if (payload_type) {
        auto* llvm_struct = llvm::StructType::create(
            context.llvm_context(), {tag_type, payload_type}, name);
        return llvm_struct;
      }
      // Simple enum (no payload): { i64 }.
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
