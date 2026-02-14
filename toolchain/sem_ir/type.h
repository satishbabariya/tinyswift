// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef TINYSWIFT_TOOLCHAIN_SEM_IR_TYPE_H_
#define TINYSWIFT_TOOLCHAIN_SEM_IR_TYPE_H_

#include "common/map.h"
#include "llvm/ADT/SmallVector.h"
#include "toolchain/base/shared_value_stores.h"
#include "toolchain/base/yaml.h"
#include "toolchain/sem_ir/ids.h"
#include "toolchain/sem_ir/inst_kind.h"

// TODO: Implement your language's type system here.
// See TinySwift compiler for reference implementation patterns.

namespace TinySwift::SemIR {

// Forward declaration.
class File;
class Inst;

// The value representation for a type describes how values of that type are
// represented at compile time and at run time.
struct ValueRepr : public Printable<ValueRepr> {
  enum Kind : int8_t {
    // An unknown representation. This is used for incomplete types.
    Unknown,
    // The type has no representation. This is used for empty types, such as
    // empty tuples and empty structs.
    None,
    // The value representation is a copy of the value. On call boundaries,
    // the value itself is passed.
    Copy,
    // The value representation is a pointer to the object representation.
    // On call boundaries, the address of the object representation is passed.
    Pointer,
    // A custom representation. Used for types whose representation is
    // determined externally, such as C++ types.
    Custom,
  };

  auto Print(llvm::raw_ostream& out) const -> void {
    out << "{kind: ";
    switch (kind) {
      case Unknown:
        out << "unknown";
        break;
      case None:
        out << "none";
        break;
      case Copy:
        out << "copy";
        break;
      case Pointer:
        out << "pointer";
        break;
      case Custom:
        out << "custom";
        break;
    }
    out << ", type: " << type_id << "}";
  }

  Kind kind = Unknown;
  TypeId type_id = TypeId::None;
};

// Information about a complete type.
struct CompleteTypeInfo {
  ValueRepr value_repr = {.kind = ValueRepr::Unknown};
  ClassId abstract_class_id = ClassId::None;
};

// Provides a ValueStore wrapper with an API specific to types.
class TypeStore : public Yaml::Printable<TypeStore> {
 public:
  // Used to return information about an integer type.
  struct IntTypeInfo {
    bool is_signed;
    IntId bit_width;
  };

  explicit TypeStore(File* file) : file_(file) {}

  // Returns the ID of the constant used to define the specified type.
  auto GetConstantId(TypeId type_id) const -> ConstantId;

  // Returns the type ID for a constant that is a type value.
  auto GetTypeIdForTypeConstantId(ConstantId constant_id) const -> TypeId;

  // Returns the type ID for an instruction whose constant value is a type.
  auto GetTypeIdForTypeInstId(InstId inst_id) const -> TypeId;
  auto GetTypeIdForTypeInstId(TypeInstId inst_id) const -> TypeId;

  // Converts an `InstId` to a `TypeInstId`.
  auto GetAsTypeInstId(InstId inst_id) const -> TypeInstId;

  // Returns the ID of the instruction used to define the specified type.
  auto GetTypeInstId(TypeId type_id) const -> TypeInstId;

  // Returns the instruction used to define the specified type.
  auto GetAsInst(TypeId type_id) const -> Inst;

  // Returns the unattached form of the given type.
  auto GetUnattachedType(TypeId type_id) const -> TypeId;

  // Returns whether the specified kind of instruction was used to define
  // the type.
  template <typename InstT>
  auto Is(TypeId type_id) const -> bool;

  // Returns the instruction used to define the specified type, which is known
  // to be a particular kind.
  template <typename InstT>
  auto GetAs(TypeId type_id) const -> InstT;

  // Returns the instruction used to define the specified type, if it is of a
  // particular kind.
  template <typename InstT>
  auto TryGetAs(TypeId type_id) const -> std::optional<InstT>;

  // Gets the value representation to use for a type.
  auto GetValueRepr(TypeId type_id) const -> ValueRepr {
    if (auto type_info = complete_type_info_.Lookup(type_id)) {
      return type_info.value().value_repr;
    }
    return {.kind = ValueRepr::Unknown};
  }

  // Gets the `CompleteTypeInfo` for a type.
  auto GetCompleteTypeInfo(TypeId type_id) const -> CompleteTypeInfo {
    if (auto type_info = complete_type_info_.Lookup(type_id)) {
      return type_info.value();
    }
    return {.value_repr = {.kind = ValueRepr::Unknown}};
  }

  // Sets the `CompleteTypeInfo` associated with a type.
  auto SetComplete(TypeId type_id, const CompleteTypeInfo& info) -> void {
    TINYSWIFT_CHECK(info.value_repr.kind != ValueRepr::Unknown);
    auto insert_info = complete_type_info_.Insert(type_id, info);
    TINYSWIFT_CHECK(insert_info.is_inserted(), "Type {0} completed more than once",
                 type_id);
    complete_types_.push_back(type_id);
  }

  // Determines whether the given type is known to be complete.
  auto IsComplete(TypeId type_id) const -> bool {
    return complete_type_info_.Contains(type_id);
  }

  auto complete_types() const -> llvm::ArrayRef<TypeId> {
    return complete_types_;
  }

  auto OutputYaml() const -> Yaml::OutputMapping {
    return Yaml::OutputMapping([&](Yaml::OutputMapping::Map map) {
      for (auto type_id : complete_types_) {
        auto info = GetCompleteTypeInfo(type_id);
        map.Add(PrintToString(type_id),
                Yaml::OutputMapping([&](Yaml::OutputMapping::Map map2) {
                  map2.Add("value_repr", Yaml::OutputScalar(info.value_repr));
                }));
      }
    });
  }

  auto CollectMemUsage(MemUsage& mem_usage, llvm::StringRef label) const
      -> void {
    mem_usage.Collect(MemUsage::ConcatLabel(label, "complete_type_info_"),
                      complete_type_info_);
    mem_usage.Collect(MemUsage::ConcatLabel(label, "complete_types_"),
                      complete_types_);
  }

 private:
  File* file_;
  Map<TypeId, CompleteTypeInfo> complete_type_info_;
  llvm::SmallVector<TypeId> complete_types_;
};

}  // namespace TinySwift::SemIR

#endif  // TINYSWIFT_TOOLCHAIN_SEM_IR_TYPE_H_
