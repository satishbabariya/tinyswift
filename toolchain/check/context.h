// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef TINYSWIFT_TOOLCHAIN_CHECK_CONTEXT_H_
#define TINYSWIFT_TOOLCHAIN_CHECK_CONTEXT_H_

#include <algorithm>

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "toolchain/check/check.h"
#include "toolchain/diagnostics/diagnostic.h"
#include "toolchain/diagnostics/emitter.h"
#include "toolchain/lex/tokenized_buffer.h"
#include "toolchain/parse/tree.h"
#include "toolchain/parse/tree_and_subtrees.h"
#include "toolchain/sem_ir/file.h"
#include "toolchain/sem_ir/ids.h"
#include "toolchain/sem_ir/inst.h"
#include "toolchain/sem_ir/typed_insts.h"

namespace TinySwift::Check {

// Semantic diagnostic definitions.
// NOLINTNEXTLINE(readability-identifier-naming)
TINYSWIFT_DIAGNOSTIC(TypeMismatch, Error, "type mismatch: expected {0}, got {1}",
                     std::string, std::string);
TINYSWIFT_DIAGNOSTIC(UndefinedName, Error, "use of undefined name '{0}'",
                     std::string);
TINYSWIFT_DIAGNOSTIC(RedefinedName, Error, "redefinition of '{0}'",
                     std::string);
TINYSWIFT_DIAGNOSTIC(InvalidOperandTypes, Error,
                     "invalid operand types for '{0}': '{1}' and '{2}'",
                     std::string, std::string, std::string);
TINYSWIFT_DIAGNOSTIC(MissingReturn, Error,
                     "missing return in function expected to return '{0}'",
                     std::string);
TINYSWIFT_DIAGNOSTIC(TooManyArguments, Error,
                     "too many arguments: expected {0}, got {1}",
                     int, int);
TINYSWIFT_DIAGNOSTIC(TooFewArguments, Error,
                     "too few arguments: expected {0}, got {1}",
                     int, int);
TINYSWIFT_DIAGNOSTIC(CannotCallNonFunction, Error,
                     "cannot call non-function value");
TINYSWIFT_DIAGNOSTIC(AmbiguousType, Error,
                     "type is ambiguous in this context");
TINYSWIFT_DIAGNOSTIC(InvalidMemberAccess, Error,
                     "value of type '{0}' has no member '{1}'",
                     std::string, std::string);
TINYSWIFT_DIAGNOSTIC(CannotInferType, Error,
                     "cannot infer type for '{0}'", std::string);
TINYSWIFT_DIAGNOSTIC(ArgumentLabelMismatch, Error,
                     "incorrect argument label '{0}' in call", std::string);
TINYSWIFT_DIAGNOSTIC(UnknownStructField, Error,
                     "value of type does not have field '{0}'", std::string);
TINYSWIFT_DIAGNOSTIC(MissingStructField, Error,
                     "missing field '{0}' in struct initialization",
                     std::string);

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

  // Finalizes the current top block and switches emission to a new block.
  // Used when control flow splits (if/while) and we need to continue in a
  // different block after the split (the merge/continuation block).
  auto SwitchInstBlock(SemIR::InstBlockId new_block_id) -> void;

  // Returns the current inst block id being built (top of stack).
  auto CurrentInstBlockId() const -> SemIR::InstBlockId;

  // Returns true if the current instruction block ends with a terminator
  // (Return or Branch). Used to avoid emitting dead branches after return
  // statements in switch/if bodies.
  auto IsCurrentBlockTerminated() -> bool;

  // Adds an instruction ID to a specific block.
  auto AddInstToBlock(SemIR::InstBlockId block_id, SemIR::InstId inst_id)
      -> void;

  // Pushes an existing placeholder block onto the instruction block stack.
  // Unlike PushInstBlock(), this reuses an existing block ID rather than
  // creating a new one. Used for loop body blocks so that nested control
  // flow (via SwitchInstBlock) correctly finalizes the right block.
  auto PushInstBlockWithId(SemIR::InstBlockId block_id) -> void;

  // --- Current function tracking ---

  // Sets the current function being checked (for adding body blocks).
  auto SetCurrentFunction(SemIR::FunctionId id) -> void {
    current_function_id_ = id;
  }
  auto ClearCurrentFunction() -> void {
    current_function_id_ = SemIR::FunctionId::None;
  }
  auto CurrentFunctionId() const -> SemIR::FunctionId {
    return current_function_id_;
  }

  // Adds a body block to the current function.
  auto AddBodyBlock(SemIR::InstBlockId block_id) -> void;

  // --- Loop context (for break/continue) ---

  // Tracks the break and continue targets for the current loop.
  struct LoopContext {
    SemIR::InstBlockId break_id;     // merge block (break target)
    SemIR::InstBlockId continue_id;  // cond block (continue target)
  };

  // Pushes a new loop context onto the loop stack.
  auto PushLoopContext(SemIR::InstBlockId break_id,
                       SemIR::InstBlockId continue_id) -> void;

  // Pops the innermost loop context.
  auto PopLoopContext() -> void;

  // Returns the innermost loop context, or nullptr if not in a loop.
  auto CurrentLoop() const -> const LoopContext*;

  // --- Pending condition for if/while/guard ---
  // The parser places condition expressions as siblings before the statement
  // node. HandleCodeBlock stores the parse NodeId here for the statement
  // handler to evaluate (if/guard evaluate inline; while evaluates in a
  // dedicated condition block so the back-edge can re-evaluate it).
  auto SetPendingCondition(Parse::NodeId node_id) -> void {
    pending_condition_node_id_ = node_id;
  }
  auto TakePendingCondition() -> Parse::NodeId {
    auto id = pending_condition_node_id_;
    pending_condition_node_id_ = Parse::NodeId::None;
    return id;
  }

  // --- Pending optional binding name for if-let ---
  // When HandleCodeBlock sees `IdentifierPattern OBC IfStatement`, it stores
  // the IdentifierPattern node here so HandleIfStatement can bind the name.
  auto SetPendingOptName(Parse::NodeId node_id) -> void {
    pending_opt_name_node_id_ = node_id;
  }
  auto TakePendingOptName() -> Parse::NodeId {
    auto id = pending_opt_name_node_id_;
    pending_opt_name_node_id_ = Parse::NodeId::None;
    return id;
  }

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

  // Returns children of a node in source order. The TreeAndSubtrees::children()
  // iterator returns reverse postorder (which is reverse source order for
  // siblings), so we collect and reverse.
  auto children_source_order(Parse::NodeId n) const
      -> llvm::SmallVector<Parse::NodeId> {
    llvm::SmallVector<Parse::NodeId> result;
    for (auto child : tree_and_subtrees_->children(n)) {
      result.push_back(child);
    }
    std::reverse(result.begin(), result.end());
    return result;
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
  auto has_errors() const -> bool { return sem_ir_->has_errors(); }

  // --- Diagnostics ---

  // Emits a diagnostic at the given parse node location and marks has_errors.
  template <typename... Args>
  auto EmitError(Parse::NodeId node_id,
                 const Diagnostics::DiagnosticBase<Args...>& diagnostic,
                 Diagnostics::Internal::NoTypeDeduction<Args>... args) -> void {
    auto token = node_token(node_id);
    emitter_.Emit(token, diagnostic, args...);
    set_has_errors(true);
  }

  // Emits a diagnostic at the given token and marks has_errors.
  template <typename... Args>
  auto EmitErrorAt(Lex::TokenIndex token,
                   const Diagnostics::DiagnosticBase<Args...>& diagnostic,
                   Diagnostics::Internal::NoTypeDeduction<Args>... args) -> void {
    emitter_.Emit(token, diagnostic, args...);
    set_has_errors(true);
  }

  // --- Type name helpers ---

  // Returns a human-readable name for a TypeId.
  auto GetTypeName(SemIR::TypeId type_id) -> std::string;

 private:
  // Diagnostic emitter for semantic analysis, locating by token index.
  class TokenEmitter : public Diagnostics::Emitter<Lex::TokenIndex> {
   public:
    explicit TokenEmitter(Diagnostics::Consumer* consumer,
                          const Lex::TokenizedBuffer* tokens)
        : Emitter(consumer), tokens_(tokens) {}

   protected:
    auto ConvertLoc(Lex::TokenIndex token, ContextFnT /*context_fn*/) const
        -> Diagnostics::ConvertedLoc override {
      return tokens_->TokenToDiagnosticLoc(token);
    }

   private:
    const Lex::TokenizedBuffer* tokens_;
  };

  SemIR::File* sem_ir_;
  const Parse::TreeAndSubtrees* tree_and_subtrees_;
  TokenEmitter emitter_;

  // Stack of inst block builders. Each entry is a (placeholder_id, insts) pair.
  struct InstBlockEntry {
    SemIR::InstBlockId id;
    llvm::SmallVector<SemIR::InstId> insts;
  };
  llvm::SmallVector<InstBlockEntry> inst_block_stack_;

  // Stack of name scopes.
  llvm::SmallVector<ScopeEntry> scope_stack_;

  // Current function being checked.
  SemIR::FunctionId current_function_id_ = SemIR::FunctionId::None;

  // Pending condition parse node for if/while/guard statements.
  Parse::NodeId pending_condition_node_id_ = Parse::NodeId::None;

  // Pending optional binding name node for if-let statements.
  Parse::NodeId pending_opt_name_node_id_ = Parse::NodeId::None;

  // Stack of active loop contexts (for break/continue targets).
  llvm::SmallVector<LoopContext> loop_stack_;
};

}  // namespace TinySwift::Check

#endif  // TINYSWIFT_TOOLCHAIN_CHECK_CONTEXT_H_
