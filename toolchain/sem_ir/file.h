// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef TINYSWIFT_TOOLCHAIN_SEM_IR_FILE_H_
#define TINYSWIFT_TOOLCHAIN_SEM_IR_FILE_H_

#include "common/error.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/Allocator.h"
#include "llvm/Support/FormatVariadic.h"
#include "toolchain/base/shared_value_stores.h"
#include "toolchain/base/value_store.h"
#include "toolchain/base/yaml.h"
#include "toolchain/parse/tree.h"
#include "toolchain/sem_ir/function.h"
#include "toolchain/sem_ir/ids.h"
#include "toolchain/sem_ir/inst.h"
#include "toolchain/sem_ir/singleton_insts.h"
#include "toolchain/sem_ir/type.h"

// TODO: Implement your language's semantic IR file representation here.
// See TinySwift compiler for reference implementation patterns.

namespace TinySwift::SemIR {

// Stores constant values for instructions. Maps InstId -> ConstantId.
class ConstantValueStore : public Yaml::Printable<ConstantValueStore> {
 public:
  explicit ConstantValueStore(ConstantId default_value, InstStore* insts)
      : default_(default_value), insts_(insts) {}

  // Returns the constant value for an instruction.
  auto Get(InstId inst_id) const -> ConstantId {
    auto index = insts_->GetRawIndex(inst_id);
    if (static_cast<size_t>(index) >= values_.size()) {
      return default_;
    }
    return values_[index];
  }

  // Sets the constant value for an instruction.
  auto Set(InstId inst_id, ConstantId value) -> void {
    auto index = insts_->GetRawIndex(inst_id);
    if (static_cast<size_t>(index) >= values_.size()) {
      values_.resize(index + 1, default_);
    }
    values_[index] = value;
  }

  // Returns the instruction ID for a concrete constant.
  auto GetInstId(ConstantId constant_id) const -> InstId {
    if (!constant_id.has_value() || !constant_id.is_constant()) {
      return InstId::None;
    }
    if (constant_id.is_concrete()) {
      return constant_id.concrete_inst_id();
    }
    // TODO: Handle symbolic constants.
    return InstId::None;
  }

  // Returns the constant instruction ID for the given instruction.
  auto GetConstantInstId(InstId inst_id) const -> InstId {
    return GetInstId(Get(inst_id));
  }

  // Returns the instruction ID if the constant is valid, otherwise None.
  auto GetInstIdIfValid(ConstantId constant_id) const -> InstId {
    if (!constant_id.has_value() || !constant_id.is_constant()) {
      return InstId::None;
    }
    return GetInstId(constant_id);
  }

  // Returns the unattached form of a constant. For the boilerplate, this
  // just returns the constant as-is.
  auto GetUnattachedConstant(ConstantId constant_id) const -> ConstantId {
    // TODO: Handle symbolic constants with attached types.
    return constant_id;
  }

  auto OutputYaml(bool /*include_singletons*/ = false) const
      -> Yaml::OutputMapping {
    return Yaml::OutputMapping([&](Yaml::OutputMapping::Map map) {
      for (size_t i = 0; i < values_.size(); ++i) {
        if (values_[i].has_value() && values_[i].is_constant()) {
          map.Add(PrintToString(InstId(static_cast<int32_t>(i))),
                  Yaml::OutputScalar(values_[i]));
        }
      }
    });
  }

  auto CollectMemUsage(MemUsage& mem_usage, llvm::StringRef label) const
      -> void {
    mem_usage.Collect(MemUsage::ConcatLabel(label, "values_"), values_);
  }

 private:
  ConstantId default_;
  InstStore* insts_;
  llvm::SmallVector<ConstantId> values_;
};

// Stores computed global constants (e.g. types) with hash-consing for
// deduplication.  Two identical type constants will share the same InstId.
class ConstantStore {
 public:
  explicit ConstantStore(File* /*file*/) {}

  // Look up or insert a constant instruction.  Returns the canonical InstId
  // for the given key.  `key` should be a stable hash (e.g., inst kind +
  // operand IDs).  If a constant with the same key already exists, the
  // existing InstId is returned (deduplication).
  auto GetOrAdd(uint64_t key, InstId inst_id) -> InstId {
    auto [it, inserted] = dedup_map_.try_emplace(key, inst_id);
    if (!inserted) {
      return it->second;  // Already exists — return deduplicated ID.
    }
    insts_.push_back(inst_id);
    return inst_id;
  }

  auto CollectMemUsage(MemUsage& mem_usage, llvm::StringRef label) const
      -> void {
    mem_usage.Collect(MemUsage::ConcatLabel(label, "insts_"), insts_);
  }

 private:
  llvm::SmallVector<InstId> insts_;
  llvm::DenseMap<uint64_t, InstId> dedup_map_;
};

// An entity name binding.
struct EntityName : public Printable<EntityName> {
  NameId name_id;
  NameScopeId parent_scope_id;

  auto Print(llvm::raw_ostream& out) const -> void {
    out << "{name: " << name_id << ", parent_scope: " << parent_scope_id << "}";
  }
};

using EntityNameStore = ValueStore<EntityNameId, EntityName, Tag<CheckIRId>>;

// A scope in which names can be looked up.
struct NameScope : public Printable<NameScope> {
  NameId name_id;
  NameScopeId parent_scope_id;
  InstId inst_id;

  // Persistent name→InstId map for member lookup after checking.
  llvm::DenseMap<int32_t, InstId> names;

  auto Print(llvm::raw_ostream& out) const -> void {
    out << "{name: " << name_id << ", parent_scope: " << parent_scope_id << "}";
  }
};

// Stores name scopes.
class NameScopeStore {
 public:
  explicit NameScopeStore(File* /*file*/) {}

  auto Add(InstId inst_id, NameId name_id, NameScopeId parent_scope_id)
      -> NameScopeId {
    auto id = NameScopeId(static_cast<int32_t>(scopes_.size()));
    NameScope scope{
        .name_id = name_id,
        .parent_scope_id = parent_scope_id,
        .inst_id = inst_id,
        .names = llvm::DenseMap<int32_t, InstId>()};
    scopes_.push_back(std::move(scope));
    return id;
  }

  auto Get(NameScopeId id) const -> const NameScope& { return scopes_[id.index]; }
  auto Get(NameScopeId id) -> NameScope& { return scopes_[id.index]; }

  auto OutputYaml() const -> Yaml::OutputMapping {
    return Yaml::OutputMapping([&](Yaml::OutputMapping::Map map) {
      for (size_t i = 0; i < scopes_.size(); ++i) {
        map.Add(PrintToString(NameScopeId(static_cast<int32_t>(i))),
                Yaml::OutputScalar(scopes_[i]));
      }
    });
  }

  auto CollectMemUsage(MemUsage& mem_usage, llvm::StringRef label) const
      -> void {
    mem_usage.Collect(MemUsage::ConcatLabel(label, "scopes_"), scopes_);
  }

 private:
  llvm::SmallVector<NameScope> scopes_;
};

// The semantic IR for a single file.
class File : public Printable<File> {
 public:
  // Starts a new file for Check::CheckParseTree.
  explicit File(const Parse::Tree* parse_tree, CheckIRId check_ir_id,
                SharedValueStores& value_stores, std::string filename);

  File(const File&) = delete;
  ~File();
  auto operator=(const File&) -> File& = delete;

  // Verifies that invariants of the semantics IR hold.
  auto Verify() const -> ErrorOr<Success>;

  // Prints the full IR.
  auto Print(llvm::raw_ostream& out, bool include_singletons = false) const
      -> void {
    Yaml::Print(out, OutputYaml(include_singletons));
  }
  auto OutputYaml(bool include_singletons) const -> Yaml::OutputMapping;

  // Collects memory usage of members.
  auto CollectMemUsage(MemUsage& mem_usage, llvm::StringRef label) const
      -> void;

  // Gets the pointee type of the given type, which must be a pointer type.
  auto GetPointeeType(TypeId pointer_id) const -> TypeId {
    return types().GetTypeIdForTypeInstId(
        types().GetAs<PointerType>(pointer_id).pointee_id);
  }

  auto check_ir_id() const -> CheckIRId { return check_ir_id_; }

  // Directly expose SharedValueStores members.
  auto identifiers() -> SharedValueStores::IdentifierStore& {
    return value_stores_->identifiers();
  }
  auto identifiers() const -> const SharedValueStores::IdentifierStore& {
    return value_stores_->identifiers();
  }
  auto ints() -> SharedValueStores::IntStore& { return value_stores_->ints(); }
  auto ints() const -> const SharedValueStores::IntStore& {
    return value_stores_->ints();
  }
  auto reals() -> SharedValueStores::RealStore& {
    return value_stores_->reals();
  }
  auto reals() const -> const SharedValueStores::RealStore& {
    return value_stores_->reals();
  }
  auto floats() -> SharedValueStores::FloatStore& {
    return value_stores_->floats();
  }
  auto floats() const -> const SharedValueStores::FloatStore& {
    return value_stores_->floats();
  }
  auto string_literal_values() -> SharedValueStores::StringLiteralStore& {
    return value_stores_->string_literal_values();
  }
  auto string_literal_values() const
      -> const SharedValueStores::StringLiteralStore& {
    return value_stores_->string_literal_values();
  }

  // Allocates a persistent copy of a string in the compile-scoped allocator.
  auto AllocateString(llvm::StringRef s) -> llvm::StringRef {
    return value_stores_->AllocateString(s);
  }

  auto entity_names() -> EntityNameStore& { return entity_names_; }
  auto entity_names() const -> const EntityNameStore& { return entity_names_; }
  auto functions() -> FunctionStore& { return functions_; }
  auto functions() const -> const FunctionStore& { return functions_; }
  auto name_scopes() -> NameScopeStore& { return name_scopes_; }
  auto name_scopes() const -> const NameScopeStore& { return name_scopes_; }
  auto types() -> TypeStore& { return types_; }
  auto types() const -> const TypeStore& { return types_; }
  auto insts() -> InstStore& { return insts_; }
  auto insts() const -> const InstStore& { return insts_; }
  auto constant_values() -> ConstantValueStore& { return constant_values_; }
  auto constant_values() const -> const ConstantValueStore& {
    return constant_values_;
  }
  auto inst_blocks() -> InstBlockStore& { return inst_blocks_; }
  auto inst_blocks() const -> const InstBlockStore& { return inst_blocks_; }
  auto constants() -> ConstantStore& { return constants_; }
  auto constants() const -> const ConstantStore& { return constants_; }

  auto top_inst_block_id() const -> InstBlockId { return top_inst_block_id_; }
  auto set_top_inst_block_id(InstBlockId block_id) -> void {
    top_inst_block_id_ = block_id;
  }
  auto global_ctor_id() const -> FunctionId { return global_ctor_id_; }
  auto set_global_ctor_id(FunctionId function_id) -> void {
    global_ctor_id_ = function_id;
  }

  // Returns true if there were errors creating the semantics IR.
  auto has_errors() const -> bool { return has_errors_; }
  auto set_has_errors(bool has_errors) -> void { has_errors_ = has_errors; }

  auto filename() const -> llvm::StringRef { return filename_; }

  auto parse_tree() const -> const Parse::Tree& { return *parse_tree_; }

  // M97: Cycle-capable type tracking.
  auto MarkCycleCapableType(TypeId type_id) -> void {
    cycle_capable_types_.insert(type_id.index);
  }
  auto IsCycleCapableType(TypeId type_id) const -> bool {
    return cycle_capable_types_.count(type_id.index) > 0;
  }

 private:
  const Parse::Tree* parse_tree_;

  // True if parts of the IR may be invalid.
  bool has_errors_ = false;

  // The file's ID.
  CheckIRId check_ir_id_;

  // Shared, compile-scoped values.
  SharedValueStores* value_stores_;

  // Slab allocator, used to allocate instruction and type blocks.
  llvm::BumpPtrAllocator allocator_;

  // The associated filename.
  std::string filename_;

  // Storage for entity names.
  EntityNameStore entity_names_;

  // Storage for callable objects.
  FunctionStore functions_;

  // All instructions. The first entries will always be the singleton
  // instructions.
  InstStore insts_;

  // Storage for name scopes.
  NameScopeStore name_scopes_ = NameScopeStore(this);

  // Constant values for instructions.
  ConstantValueStore constant_values_;

  // Instruction blocks within the IR. These reference entries in
  // insts_. Storage for the data is provided by allocator_.
  InstBlockStore inst_blocks_;

  // The top instruction block ID.
  InstBlockId top_inst_block_id_ = InstBlockId::None;

  // The global constructor function id.
  FunctionId global_ctor_id_ = FunctionId::None;

  // Storage for instructions that represent computed global constants, such as
  // types.
  ConstantStore constants_;

  // Descriptions of types used in this file.
  TypeStore types_ = TypeStore(this);

  // M97: Set of TypeId indices for class types that can form reference cycles.
  llvm::DenseSet<int32_t> cycle_capable_types_;
};

}  // namespace TinySwift::SemIR

#endif  // TINYSWIFT_TOOLCHAIN_SEM_IR_FILE_H_
