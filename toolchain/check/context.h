// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef TINYSWIFT_TOOLCHAIN_CHECK_CONTEXT_H_
#define TINYSWIFT_TOOLCHAIN_CHECK_CONTEXT_H_

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "toolchain/check/check.h"
#include "toolchain/diagnostics/emitter.h"
#include "toolchain/lex/tokenized_buffer.h"
#include "toolchain/parse/tree.h"
#include "toolchain/parse/tree_and_subtrees.h"
#include "toolchain/sem_ir/file.h"
#include "toolchain/sem_ir/ids.h"
#include "toolchain/sem_ir/inst.h"
#include "toolchain/sem_ir/typed_insts.h"

namespace TinySwift::Check {

// Manages the semantic checking context for a single file.
class Context {
 public:
  Context(Unit& unit, const Parse::TreeAndSubtrees& tree_and_subtrees,
          Diagnostics::Consumer& consumer);

  // --- SemIR store access ---
  auto sem_ir() -> SemIR::File& { return *sem_ir_; }
  auto sem_ir() const -> const SemIR::File& { return *sem_ir_; }
  auto insts() -> SemIR::InstStore& { return sem_ir_->insts(); }
  auto types() -> SemIR::TypeStore& { return sem_ir_->types(); }
  auto functions() -> SemIR::FunctionStore& { return sem_ir_->functions(); }
  auto name_scopes() -> SemIR::NameScopeStore& {
    return sem_ir_->name_scopes();
  }
  auto inst_blocks() -> SemIR::InstBlockStore& {
    return sem_ir_->inst_blocks();
  }
  auto entity_names() -> SemIR::EntityNameStore& {
    return sem_ir_->entity_names();
  }

  // --- Instruction emission ---

  // Adds an instruction without adding it to the current block.
  auto AddInstInNoBlock(SemIR::LocIdAndInst loc_and_inst) -> SemIR::InstId;

  // Adds an instruction and appends it to the current block.
  auto AddInst(SemIR::LocIdAndInst loc_and_inst) -> SemIR::InstId;

  // --- Inst block management (stack-based) ---

  // Pushes a new instruction block and returns its placeholder ID.
  auto PushInstBlock() -> SemIR::InstBlockId;

  // Pops the current instruction block, finalizes it, and returns its ID.
  auto PopInstBlock() -> SemIR::InstBlockId;

  // Returns the current inst block id being built (top of stack).
  auto CurrentInstBlockId() const -> SemIR::InstBlockId;

  // Adds an instruction ID to a specific block.
  auto AddInstToBlock(SemIR::InstBlockId block_id, SemIR::InstId inst_id)
      -> void;

  // --- Name scope management ---

  // A name-to-InstId mapping for a scope.
  struct ScopeEntry {
    explicit ScopeEntry(SemIR::NameScopeId scope_id) : scope_id(scope_id) {}
    SemIR::NameScopeId scope_id;
    llvm::DenseMap<int32_t, SemIR::InstId> names;  // NameId.index -> InstId
  };

  // Pushes a new name scope.
  auto PushScope(SemIR::NameScopeId scope_id) -> void;

  // Pops the current name scope.
  auto PopScope() -> void;

  // Looks up a name in the scope stack (innermost first).
  auto LookupName(SemIR::NameId name_id) -> SemIR::InstId;

  // Adds a name binding to the current scope.
  auto AddNameToScope(SemIR::NameId name_id, SemIR::InstId inst_id) -> void;

  // Returns the current scope ID.
  auto CurrentScopeId() const -> SemIR::NameScopeId;

  // --- Type resolution ---

  // Resolves a type name string to a builtin TypeId.
  auto GetBuiltinType(llvm::StringRef name) -> SemIR::TypeId;

  // --- Tree access ---
  auto tree() const -> const Parse::Tree& {
    return tree_and_subtrees_->tree();
  }
  auto tree_and_subtrees() const -> const Parse::TreeAndSubtrees& {
    return *tree_and_subtrees_;
  }
  auto tokens() const -> const Lex::TokenizedBuffer& { return tree().tokens(); }

  auto node_kind(Parse::NodeId n) const -> Parse::NodeKind {
    return tree().node_kind(n);
  }
  auto node_token(Parse::NodeId n) const -> Lex::TokenIndex {
    return tree().node_token(n);
  }
  auto token_text(Lex::TokenIndex token) const -> llvm::StringRef {
    return tokens().GetTokenText(token);
  }
  auto node_has_error(Parse::NodeId n) const -> bool {
    return tree().node_has_error(n);
  }

  // --- Shared value stores ---
  auto identifiers() -> SharedValueStores::IdentifierStore& {
    return sem_ir_->identifiers();
  }
  auto ints() -> SharedValueStores::IntStore& { return sem_ir_->ints(); }
  auto string_literal_values() -> SharedValueStores::StringLiteralStore& {
    return sem_ir_->string_literal_values();
  }
  auto floats() -> SharedValueStores::FloatStore& { return sem_ir_->floats(); }

  // --- Error tracking ---
  auto set_has_errors(bool has_errors) -> void {
    sem_ir_->set_has_errors(has_errors);
  }

 private:
  SemIR::File* sem_ir_;
  const Parse::TreeAndSubtrees* tree_and_subtrees_;

  // Stack of inst block builders. Each entry is a (placeholder_id, insts) pair.
  struct InstBlockEntry {
    SemIR::InstBlockId id;
    llvm::SmallVector<SemIR::InstId> insts;
  };
  llvm::SmallVector<InstBlockEntry> inst_block_stack_;

  // Stack of name scopes.
  llvm::SmallVector<ScopeEntry> scope_stack_;
};

}  // namespace TinySwift::Check

#endif  // TINYSWIFT_TOOLCHAIN_CHECK_CONTEXT_H_
