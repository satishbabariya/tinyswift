// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef TINYSWIFT_TOOLCHAIN_CHECK_CONTEXT_H_
#define TINYSWIFT_TOOLCHAIN_CHECK_CONTEXT_H_

#include <algorithm>
#include <deque>

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringMap.h"
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

// Manages the semantic checking context, shared across all files in a module.
class Context {
 public:
  Context(Unit& unit, const Parse::TreeAndSubtrees& tree_and_subtrees,
          Diagnostics::Consumer& consumer);

  // --- Multi-file support (M73) ---

  // Switches the current tree and subtrees pointer (for multi-file checking).
  auto SetCurrentTreeAndSubtrees(const Parse::TreeAndSubtrees& tree_and_subtrees) -> void;

  // Sets the store of all file trees for cross-file generic instantiation.
  auto SetAllTrees(const Parse::GetTreeAndSubtreesStore* store) -> void {
    all_trees_ = store;
  }

  // Returns the store of all file trees, or nullptr if single-file.
  auto all_trees() const -> const Parse::GetTreeAndSubtreesStore* {
    return all_trees_;
  }

  // Tracks which file is currently being processed.
  auto SetCurrentFile(SemIR::CheckIRId id) -> void { current_file_id_ = id; }
  auto current_file_id() const -> SemIR::CheckIRId { return current_file_id_; }

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

  // Tracks the enclosing struct/class type during HandleTypeMembers.
  // Set before processing type members so methods can synthesize `self`.
  auto SetCurrentType(SemIR::InstId type_inst_id) -> void {
    current_type_id_ = type_inst_id;
  }
  auto ClearCurrentType() -> void { current_type_id_ = SemIR::InstId::None; }
  auto CurrentTypeInstId() const -> SemIR::InstId { return current_type_id_; }

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

  // --- Pending access level (M74) ---
  // When an AccessModifier node is encountered as a sibling before a
  // declaration, store the access level for the next declaration to consume.
  auto SetPendingAccessLevel(SemIR::AccessLevel level) -> void {
    pending_access_level_ = level;
    has_pending_access_level_ = true;
  }
  auto TakePendingAccessLevel() -> SemIR::AccessLevel {
    if (has_pending_access_level_) {
      has_pending_access_level_ = false;
      return pending_access_level_;
    }
    return SemIR::AccessLevel::Internal;  // default
  }

  // --- Pending attribute for @extern/@cdecl (M75/M76) ---
  // When an Attribute node is encountered as a sibling before a declaration,
  // store the parse node for the declaration handler to extract.
  auto SetPendingAttribute(Parse::NodeId node_id) -> void {
    pending_attribute_node_id_ = node_id;
  }
  auto TakePendingAttribute() -> Parse::NodeId {
    auto id = pending_attribute_node_id_;
    pending_attribute_node_id_ = Parse::NodeId::None;
    return id;
  }

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

  // --- Pending LHS for type cast expressions (M39) ---
  // HandleCodeBlock stores the LHS InstId when an Expr is followed by a cast
  // node (IsExpr/AsExpr/AsQuestionExpr/AsExclaimExpr).
  auto SetPendingCastExpr(SemIR::InstId id) -> void {
    pending_cast_expr_ = id;
  }
  auto TakePendingCastExpr() -> SemIR::InstId {
    auto id = pending_cast_expr_;
    pending_cast_expr_ = SemIR::InstId::None;
    return id;
  }

  // M68: Pending callee for generic calls. When generic args shift the callee
  // IdentifierNameExpr out of CallExpr's subtree, the parent context sets this.
  auto SetPendingCalleeId(SemIR::InstId id) -> void {
    pending_callee_id_ = id;
  }
  auto TakePendingCalleeId() -> SemIR::InstId {
    auto id = pending_callee_id_;
    pending_callee_id_ = SemIR::InstId::None;
    return id;
  }

  // M80: Pending generic argument clause from GenericSpecExpr.
  auto SetPendingGenericArgClause(Parse::NodeId node_id) -> void {
    pending_generic_arg_clause_ = node_id;
  }
  auto TakePendingGenericArgClause() -> Parse::NodeId {
    auto id = pending_generic_arg_clause_;
    pending_generic_arg_clause_ = Parse::NodeId::None;
    return id;
  }

  // --- M81: Error handling catch block target ---
  // When inside a do-catch, HandleTryExpr branches to this catch block on error.
  auto PushCatchBlock(SemIR::InstBlockId catch_id) -> void {
    catch_block_stack_.push_back(catch_id);
  }
  auto PopCatchBlock() -> void {
    catch_block_stack_.pop_back();
  }
  auto CurrentCatchBlock() const -> SemIR::InstBlockId {
    return catch_block_stack_.empty() ? SemIR::InstBlockId::None
                                      : catch_block_stack_.back();
  }

  // --- Deferred blocks for defer statement (M51) ---
  // `defer { ... }` pushes the CodeBlock parse node here. Each return
  // statement emits deferred blocks LIFO before its ReturnExpr.
  auto PushDeferredBlock(Parse::NodeId node_id) -> void {
    deferred_blocks_.push_back(node_id);
  }
  auto GetDeferredBlocks() const -> const llvm::SmallVector<Parse::NodeId>& {
    return deferred_blocks_;
  }
  auto ClearDeferredBlocks() -> void { deferred_blocks_.clear(); }

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

  // M66-M72: Generic type template info (struct/class/enum).
  struct GenericTypeTemplate {
    Parse::NodeId definition_node_id = Parse::NodeId::None;
    Parse::NodeId start_node_id = Parse::NodeId::None;
    llvm::SmallVector<SemIR::Function::GenericParamInfo> generic_param_names;
    SemIR::InstId original_type_inst_id = SemIR::InstId::None;
    // M73: Which file this template was defined in (for cross-file instantiation).
    SemIR::CheckIRId source_check_ir_id = SemIR::CheckIRId(0);
  };

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
  auto AllocateString(llvm::StringRef s) -> llvm::StringRef {
    return sem_ir_->AllocateString(s);
  }

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
  Diagnostics::Consumer* consumer_;  // M73: stored for emitter re-creation
  TokenEmitter emitter_;

  // M73: Multi-file state.
  const Parse::GetTreeAndSubtreesStore* all_trees_ = nullptr;
  SemIR::CheckIRId current_file_id_ = SemIR::CheckIRId(0);

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

  // Current enclosing struct/class type (set during HandleTypeMembers).
  SemIR::InstId current_type_id_ = SemIR::InstId::None;

  // M74: Pending access level from AccessModifier sibling.
  SemIR::AccessLevel pending_access_level_ = SemIR::AccessLevel::Internal;
  bool has_pending_access_level_ = false;

  // M75/M76: Pending attribute parse node from Attribute sibling.
  Parse::NodeId pending_attribute_node_id_ = Parse::NodeId::None;

  // Pending condition parse node for if/while/guard statements.
  Parse::NodeId pending_condition_node_id_ = Parse::NodeId::None;

  // Pending optional binding name node for if-let statements.
  Parse::NodeId pending_opt_name_node_id_ = Parse::NodeId::None;

  // Pending LHS InstId for type cast expressions (M39 — is/as?/as!/as).
  SemIR::InstId pending_cast_expr_ = SemIR::InstId::None;

  // M68: Pending callee InstId for generic calls where the callee is outside
  // CallExpr's subtree (orphaned by GenericArgumentClause in the parse tree).
  SemIR::InstId pending_callee_id_ = SemIR::InstId::None;

  // M80: Pending generic argument clause node from GenericSpecExpr.
  Parse::NodeId pending_generic_arg_clause_ = Parse::NodeId::None;

  // Stack of active loop contexts (for break/continue targets).
  llvm::SmallVector<LoopContext> loop_stack_;

  // M81: Stack of catch block targets for try expressions.
  llvm::SmallVector<SemIR::InstBlockId> catch_block_stack_;

  // Stack of deferred CodeBlock parse nodes (M51: defer statement).
  // Each `defer { ... }` pushes its CodeBlock here.
  // Each return statement emits these LIFO before emitting ReturnExpr/Return.
  llvm::SmallVector<Parse::NodeId> deferred_blocks_;

  // M78: ARC cleanup tracking — each scope level tracks class-typed locals.
  struct ClassCleanup {
    SemIR::InstId var_id;       // VarStorage or ValueBinding holding the class ref
    SemIR::TypeId type_id;      // ClassType
    SemIR::InstId deinit_id;    // FunctionDecl for deinit, or InstId::None
  };
  llvm::SmallVector<llvm::SmallVector<ClassCleanup>> class_cleanup_stack_;

  // M74: Access level for declarations (keyed by InstId.index).
  llvm::DenseMap<int32_t, SemIR::AccessLevel> access_level_map_;

  // M74: Scope ID that a private declaration belongs to (keyed by InstId.index).
  llvm::DenseMap<int32_t, SemIR::NameScopeId> private_decl_scope_map_;

  // Map from NameId.index to TypeId for typealias declarations.
  llvm::DenseMap<int32_t, SemIR::TypeId> typealias_map_;

  // Map from VarStorage InstId index to ArrayLiteralInit InstId.
  // Used so that var array subscript access can find the actual array alloca.
  llvm::DenseMap<int32_t, SemIR::InstId> array_var_init_map_;

  // Map from VarStorage InstId index to DynamicArrayInit InstId (M65).
  llvm::DenseMap<int32_t, SemIR::InstId> dynamic_array_var_map_;

  // M66-M72: Generics state.

  // Type parameter bindings during specialization: NameId.index -> TypeId.
  llvm::DenseMap<int32_t, SemIR::TypeId> type_param_bindings_;
  bool is_specializing_ = false;

  // Cache: mangled name -> specialized FunctionId.
  llvm::StringMap<SemIR::FunctionId> specialization_cache_;

  llvm::DenseMap<int32_t, GenericTypeTemplate> generic_type_templates_;  // NameId.index -> template

  // Type specialization cache: mangled name -> specialized type InstId.
  llvm::StringMap<SemIR::InstId> type_specialization_cache_;

  // Persistent storage for mangled names (avoids dangling StringRefs).
  std::deque<std::string> generic_name_storage_;

 public:
  // M74: Set access level for a declaration InstId.
  auto SetAccessLevel(SemIR::InstId inst_id, SemIR::AccessLevel level,
                      SemIR::NameScopeId declaring_scope = SemIR::NameScopeId::None) -> void {
    access_level_map_.insert_or_assign(inst_id.index, level);
    if (level == SemIR::AccessLevel::Private && declaring_scope.has_value()) {
      private_decl_scope_map_.insert_or_assign(inst_id.index, declaring_scope);
    }
  }

  // M74: Get access level for a declaration InstId.
  auto GetAccessLevel(SemIR::InstId inst_id) -> SemIR::AccessLevel {
    auto it = access_level_map_.find(inst_id.index);
    if (it != access_level_map_.end()) {
      return it->second;
    }
    return SemIR::AccessLevel::Internal;
  }

  auto typealias_map() -> llvm::DenseMap<int32_t, SemIR::TypeId>& {
    return typealias_map_;
  }

  // M66-M72: Generics accessors.
  auto is_specializing() const -> bool { return is_specializing_; }
  auto set_is_specializing(bool v) -> void { is_specializing_ = v; }

  auto type_param_bindings() -> llvm::DenseMap<int32_t, SemIR::TypeId>& {
    return type_param_bindings_;
  }
  auto type_param_bindings() const -> const llvm::DenseMap<int32_t, SemIR::TypeId>& {
    return type_param_bindings_;
  }

  auto specialization_cache() -> llvm::StringMap<SemIR::FunctionId>& {
    return specialization_cache_;
  }

  auto generic_type_templates() -> llvm::DenseMap<int32_t, GenericTypeTemplate>& {
    return generic_type_templates_;
  }

  auto type_specialization_cache() -> llvm::StringMap<SemIR::InstId>& {
    return type_specialization_cache_;
  }

  auto generic_name_storage() -> std::deque<std::string>& {
    return generic_name_storage_;
  }

  // Register a VarStorage as holding an array initializer.
  auto SetArrayVarInit(SemIR::InstId var_id, SemIR::InstId ali_id) -> void {
    array_var_init_map_.insert({var_id.index, ali_id});
  }

  // Look up the ArrayLiteralInit for a VarStorage, or return None.
  auto GetArrayVarInit(SemIR::InstId var_id) -> SemIR::InstId {
    auto it = array_var_init_map_.find(var_id.index);
    if (it != array_var_init_map_.end()) {
      return it->second;
    }
    return SemIR::InstId::None;
  }

  // M65: Register a VarStorage as holding a DynamicArrayInit.
  auto SetDynamicArrayVar(SemIR::InstId var_id, SemIR::InstId dai_id) -> void {
    dynamic_array_var_map_.insert({var_id.index, dai_id});
  }

  // M65: Look up the DynamicArrayInit for a VarStorage, or return None.
  auto GetDynamicArrayVar(SemIR::InstId var_id) -> SemIR::InstId {
    auto it = dynamic_array_var_map_.find(var_id.index);
    if (it != dynamic_array_var_map_.end()) {
      return it->second;
    }
    return SemIR::InstId::None;
  }

  // M65: Check if a VarStorage holds a DynamicArrayInit.
  auto IsDynamicArrayVar(SemIR::InstId var_id) -> bool {
    return dynamic_array_var_map_.count(var_id.index) > 0;
  }

  // M78: ARC cleanup tracking.
  // Returns true if a TypeId is a class reference type.
  auto IsReferenceType(SemIR::TypeId type_id) -> bool;

  // Push/pop cleanup scope for function/block entry/exit.
  auto PushCleanupScope() -> void {
    class_cleanup_stack_.push_back({});
  }
  auto PopCleanupScope(Parse::NodeId loc) -> void;

  // Register a class-typed local for cleanup at scope exit.
  auto AddClassCleanup(SemIR::InstId var_id, SemIR::TypeId type_id,
                       SemIR::InstId deinit_id = SemIR::InstId::None) -> void {
    if (!class_cleanup_stack_.empty()) {
      class_cleanup_stack_.back().push_back({var_id, type_id, deinit_id});
    }
  }

  // Remove a class-typed local from cleanup (for return value ownership transfer).
  auto RemoveClassCleanup(SemIR::InstId var_id) -> void {
    if (class_cleanup_stack_.empty()) return;
    auto& scope = class_cleanup_stack_.back();
    for (auto it = scope.begin(); it != scope.end(); ++it) {
      if (it->var_id == var_id) {
        scope.erase(it);
        return;
      }
    }
  }

  // Emit releases for current scope without popping (used before return).
  auto EmitCleanupReleases(Parse::NodeId loc) -> void;

  // Look up __deinit in a class's NameScope.
  auto GetClassDeinitId(SemIR::TypeId type_id) -> SemIR::InstId;
};

}  // namespace TinySwift::Check

#endif  // TINYSWIFT_TOOLCHAIN_CHECK_CONTEXT_H_
