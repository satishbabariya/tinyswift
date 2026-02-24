// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef TINYSWIFT_TOOLCHAIN_PARSE_TYPED_NODES_H_
#define TINYSWIFT_TOOLCHAIN_PARSE_TYPED_NODES_H_

#include <optional>

#include "toolchain/lex/token_index.h"
#include "toolchain/parse/node_ids.h"
#include "toolchain/parse/node_kind.h"

namespace TinySwift::Parse {

// Helpers for defining different kinds of parse nodes.
// ----------------------------------------------------

// A pair of a list item and its optional following comma.
template <typename Element, typename Comma>
struct ListItem {
  Element value;
  std::optional<Comma> comma;
};

// A list of items, parameterized by the kind of the elements and comma.
template <typename Element, typename Comma>
using CommaSeparatedList = llvm::SmallVector<ListItem<Element, Comma>>;

// This class provides a shorthand for defining parse node kinds for leaf nodes.
template <const NodeKind& KindT, typename TokenKind,
          NodeCategory::RawEnumType Category = NodeCategory::None>
struct LeafNode {
  static constexpr auto Kind =
      KindT.Define({.category = Category, .child_count = 0});

  TokenKind token;
};

// ----------------------------------------------------------------------------
// Each node kind (in node_kind.def) should have a corresponding type defined
// here which describes the expected child structure of that parse node.
//
// See node_ids.h for the ID types used as field types.
// ----------------------------------------------------------------------------

// ============================================================================
// Error nodes
// ============================================================================

using InvalidParse = LeafNode<NodeKind::InvalidParse, Lex::TokenIndex,
                              NodeCategory::Decl | NodeCategory::Expr>;

using InvalidParseStart =
    LeafNode<NodeKind::InvalidParseStart, Lex::TokenIndex>;

struct InvalidParseSubtree {
  static constexpr auto Kind = NodeKind::InvalidParseSubtree.Define(
      {.category = NodeCategory::Decl,
       .bracketed_by = InvalidParseStart::Kind});

  InvalidParseStartId start;
  llvm::SmallVector<NodeIdNot<InvalidParseStart>> extra;
  Lex::TokenIndex token;
};

using Placeholder = LeafNode<NodeKind::Placeholder, Lex::TokenIndex>;

// ============================================================================
// File nodes
// ============================================================================

using FileStart = LeafNode<NodeKind::FileStart, Lex::FileStartTokenIndex>;
using FileEnd = LeafNode<NodeKind::FileEnd, Lex::FileEndTokenIndex>;

// ============================================================================
// General-purpose nodes
// ============================================================================

using EmptyDecl =
    LeafNode<NodeKind::EmptyDecl, Lex::SemiTokenIndex, NodeCategory::Decl>;

using IdentifierNameBeforeParams =
    LeafNode<NodeKind::IdentifierNameBeforeParams, Lex::IdentifierTokenIndex,
             NodeCategory::MemberName | NodeCategory::NonExprName>;

using IdentifierNameNotBeforeParams =
    LeafNode<NodeKind::IdentifierNameNotBeforeParams, Lex::IdentifierTokenIndex,
             NodeCategory::MemberName | NodeCategory::NonExprName>;

using IdentifierNameExpr =
    LeafNode<NodeKind::IdentifierNameExpr, Lex::IdentifierTokenIndex,
             NodeCategory::Expr>;

using SelfExpr =
    LeafNode<NodeKind::SelfExpr, Lex::SelfKeywordTokenIndex,
             NodeCategory::Expr>;

using SuperExpr =
    LeafNode<NodeKind::SuperExpr, Lex::SuperKeywordTokenIndex,
             NodeCategory::Expr>;

using DollarIdentExpr =
    LeafNode<NodeKind::DollarIdentExpr, Lex::DollarIdentTokenIndex,
             NodeCategory::Expr>;

// ============================================================================
// Import nodes
// ============================================================================

using ImportIntroducer =
    LeafNode<NodeKind::ImportIntroducer, Lex::ImportKeywordTokenIndex>;

// `import Foo`
struct ImportDecl {
  static constexpr auto Kind = NodeKind::ImportDecl.Define(
      {.category = NodeCategory::Decl,
       .bracketed_by = ImportIntroducer::Kind});

  ImportIntroducerId introducer;
  llvm::SmallVector<NodeId> path;
  Lex::TokenIndex token;
};

// ============================================================================
// Function nodes
// ============================================================================

using FunctionIntroducer =
    LeafNode<NodeKind::FunctionIntroducer, Lex::FuncKeywordTokenIndex>;

// `-> Type`
struct ReturnType {
  static constexpr auto Kind = NodeKind::ReturnType.Define({.child_count = 1});

  Lex::MinusGreaterTokenIndex token;
  AnyTypeExprId type;
};

// `func name(params) -> Type {` (the start node before the body)
struct FunctionDefinitionStart {
  static constexpr auto Kind = NodeKind::FunctionDefinitionStart.Define(
      {.bracketed_by = FunctionIntroducer::Kind});

  FunctionIntroducerId introducer;
  AnyNonExprNameId name;
  std::optional<GenericParameterClauseId> generic_params;
  std::optional<ExplicitParamListId> params;
  std::optional<ReturnTypeId> return_type;
  Lex::TokenIndex token;
};

// `func name(params) -> Type { ... }`
struct FunctionDefinition {
  static constexpr auto Kind = NodeKind::FunctionDefinition.Define(
      {.category = NodeCategory::Decl,
       .bracketed_by = FunctionDefinitionStart::Kind});

  FunctionDefinitionStartId signature;
  llvm::SmallVector<AnyStatementId> body;
  Lex::CloseCurlyBraceTokenIndex token;
};

// `func name(params) -> Type` (forward declaration - protocol member)
struct FunctionDecl {
  static constexpr auto Kind = NodeKind::FunctionDecl.Define(
      {.category = NodeCategory::Decl,
       .bracketed_by = FunctionIntroducer::Kind});

  FunctionIntroducerId introducer;
  AnyNonExprNameId name;
  std::optional<GenericParameterClauseId> generic_params;
  std::optional<ExplicitParamListId> params;
  std::optional<ReturnTypeId> return_type;
  Lex::TokenIndex token;
};

// A function parameter: `label name: Type = default`
struct FunctionParam {
  static constexpr auto Kind = NodeKind::FunctionParam.Define(
      {.category = NodeCategory::Pattern, .child_count = 2});

  std::optional<ExternalParamNameId> external_name;
  Lex::TokenIndex token;
  std::optional<TypeAnnotationId> type_annotation;
};

// External parameter name (label) in a function declaration.
using ExternalParamName =
    LeafNode<NodeKind::ExternalParamName, Lex::TokenIndex>;

// ============================================================================
// Parameter nodes
// ============================================================================

using ExplicitParamListStart =
    LeafNode<NodeKind::ExplicitParamListStart, Lex::OpenParenTokenIndex>;

using PatternListComma =
    LeafNode<NodeKind::PatternListComma, Lex::CommaTokenIndex>;

// `(a: Int, b: Int)`
struct ExplicitParamList {
  static constexpr auto Kind = NodeKind::ExplicitParamList.Define(
      {.bracketed_by = ExplicitParamListStart::Kind});

  ExplicitParamListStartId left_paren;
  CommaSeparatedList<AnyPatternId, PatternListCommaId> params;
  Lex::CloseParenTokenIndex token;
};

// ============================================================================
// Let nodes (Swift-style: no semicolons)
// ============================================================================

using LetIntroducer = LeafNode<NodeKind::LetIntroducer, Lex::LetKeywordTokenIndex>;
using LetInitializer = LeafNode<NodeKind::LetInitializer, Lex::EqualTokenIndex>;

// `name: Type`
struct LetBindingPattern {
  static constexpr auto Kind = NodeKind::LetBindingPattern.Define(
      {.category = NodeCategory::Pattern, .child_count = 2});

  IdentifierNameNotBeforeParamsId name;
  Lex::ColonTokenIndex token;
  AnyTypeExprId type;
};

// `let a: Int = 5` (no semicolon in Swift)
struct LetDecl {
  static constexpr auto Kind = NodeKind::LetDecl.Define(
      {.category = NodeCategory::Decl, .bracketed_by = LetIntroducer::Kind});

  LetIntroducerId introducer;
  AnyPatternId pattern;

  struct Initializer {
    LetInitializerId equals;
    AnyExprId initializer;
  };
  std::optional<Initializer> initializer;
  Lex::LetKeywordTokenIndex token;
};

// ============================================================================
// Var nodes (Swift-style: no semicolons)
// ============================================================================

using VariableIntroducer =
    LeafNode<NodeKind::VariableIntroducer, Lex::VarKeywordTokenIndex>;
using VariableInitializer =
    LeafNode<NodeKind::VariableInitializer, Lex::EqualTokenIndex>;

// `name: Type`
struct VarBindingPattern {
  static constexpr auto Kind = NodeKind::VarBindingPattern.Define(
      {.category = NodeCategory::Pattern, .child_count = 2});

  IdentifierNameNotBeforeParamsId name;
  Lex::ColonTokenIndex token;
  AnyTypeExprId type;
};

struct VariablePattern {
  static constexpr auto Kind = NodeKind::VariablePattern.Define(
      {.category = NodeCategory::Pattern, .child_count = 1});

  Lex::VarKeywordTokenIndex token;
  AnyPatternId inner;
};

// `var a: Int = 5` (no semicolon in Swift)
struct VariableDecl {
  static constexpr auto Kind =
      NodeKind::VariableDecl.Define({.category = NodeCategory::Decl,
                                     .bracketed_by = VariableIntroducer::Kind});

  VariableIntroducerId introducer;
  VariablePatternId pattern;

  struct Initializer {
    VariableInitializerId equals;
    AnyExprId value;
  };
  std::optional<Initializer> initializer;
  Lex::VarKeywordTokenIndex token;
};

// ============================================================================
// Type declarations
// ============================================================================

// --- Struct ---
using StructIntroducer =
    LeafNode<NodeKind::StructIntroducer, Lex::StructKeywordTokenIndex>;

struct StructDefinitionStart {
  static constexpr auto Kind = NodeKind::StructDefinitionStart.Define(
      {.bracketed_by = StructIntroducer::Kind});

  StructIntroducerId introducer;
  IdentifierNameNotBeforeParamsId name;
  std::optional<GenericParameterClauseId> generic_params;
  std::optional<InheritanceClauseId> inheritance;
  std::optional<GenericWhereClauseId> where_clause;
  Lex::OpenCurlyBraceTokenIndex token;
};

struct StructDefinition {
  static constexpr auto Kind = NodeKind::StructDefinition.Define(
      {.category = NodeCategory::Decl,
       .bracketed_by = StructDefinitionStart::Kind});

  StructDefinitionStartId start;
  llvm::SmallVector<AnyDeclId> members;
  Lex::CloseCurlyBraceTokenIndex token;
};

// --- Class ---
using ClassIntroducer =
    LeafNode<NodeKind::ClassIntroducer, Lex::ClassKeywordTokenIndex>;

struct ClassDefinitionStart {
  static constexpr auto Kind = NodeKind::ClassDefinitionStart.Define(
      {.bracketed_by = ClassIntroducer::Kind});

  ClassIntroducerId introducer;
  IdentifierNameNotBeforeParamsId name;
  std::optional<GenericParameterClauseId> generic_params;
  std::optional<InheritanceClauseId> inheritance;
  std::optional<GenericWhereClauseId> where_clause;
  Lex::OpenCurlyBraceTokenIndex token;
};

struct ClassDefinition {
  static constexpr auto Kind = NodeKind::ClassDefinition.Define(
      {.category = NodeCategory::Decl,
       .bracketed_by = ClassDefinitionStart::Kind});

  ClassDefinitionStartId start;
  llvm::SmallVector<AnyDeclId> members;
  Lex::CloseCurlyBraceTokenIndex token;
};

// --- Enum ---
using EnumIntroducer =
    LeafNode<NodeKind::EnumIntroducer, Lex::EnumKeywordTokenIndex>;

struct EnumDefinitionStart {
  static constexpr auto Kind = NodeKind::EnumDefinitionStart.Define(
      {.bracketed_by = EnumIntroducer::Kind});

  EnumIntroducerId introducer;
  IdentifierNameNotBeforeParamsId name;
  std::optional<GenericParameterClauseId> generic_params;
  std::optional<InheritanceClauseId> inheritance;
  std::optional<GenericWhereClauseId> where_clause;
  Lex::OpenCurlyBraceTokenIndex token;
};

struct EnumDefinition {
  static constexpr auto Kind = NodeKind::EnumDefinition.Define(
      {.category = NodeCategory::Decl,
       .bracketed_by = EnumDefinitionStart::Kind});

  EnumDefinitionStartId start;
  llvm::SmallVector<AnyDeclId> members;
  Lex::CloseCurlyBraceTokenIndex token;
};

using EnumCaseIntroducer =
    LeafNode<NodeKind::EnumCaseIntroducer, Lex::CaseKeywordTokenIndex>;

// A single enum case element: `red` or `red(Int)`
struct EnumCaseElement {
  static constexpr auto Kind = NodeKind::EnumCaseElement.Define(
      {.child_count = 1});

  IdentifierNameNotBeforeParamsId name;
  Lex::TokenIndex token;
};

// `case red, green, blue`
struct EnumCaseDecl {
  static constexpr auto Kind = NodeKind::EnumCaseDecl.Define(
      {.category = NodeCategory::Decl,
       .bracketed_by = EnumCaseIntroducer::Kind});

  EnumCaseIntroducerId introducer;
  CommaSeparatedList<EnumCaseElementId, PatternListCommaId> elements;
  Lex::TokenIndex token;
};

// --- Protocol ---
using ProtocolIntroducer =
    LeafNode<NodeKind::ProtocolIntroducer, Lex::ProtocolKeywordTokenIndex>;

struct ProtocolDefinitionStart {
  static constexpr auto Kind = NodeKind::ProtocolDefinitionStart.Define(
      {.bracketed_by = ProtocolIntroducer::Kind});

  ProtocolIntroducerId introducer;
  IdentifierNameNotBeforeParamsId name;
  std::optional<InheritanceClauseId> inheritance;
  std::optional<GenericWhereClauseId> where_clause;
  Lex::OpenCurlyBraceTokenIndex token;
};

struct ProtocolDefinition {
  static constexpr auto Kind = NodeKind::ProtocolDefinition.Define(
      {.category = NodeCategory::Decl,
       .bracketed_by = ProtocolDefinitionStart::Kind});

  ProtocolDefinitionStartId start;
  llvm::SmallVector<AnyDeclId> members;
  Lex::CloseCurlyBraceTokenIndex token;
};

// --- Extension ---
using ExtensionIntroducer =
    LeafNode<NodeKind::ExtensionIntroducer, Lex::ExtensionKeywordTokenIndex>;

struct ExtensionDefinitionStart {
  static constexpr auto Kind = NodeKind::ExtensionDefinitionStart.Define(
      {.bracketed_by = ExtensionIntroducer::Kind});

  ExtensionIntroducerId introducer;
  AnyTypeExprId type;
  std::optional<InheritanceClauseId> inheritance;
  std::optional<GenericWhereClauseId> where_clause;
  Lex::OpenCurlyBraceTokenIndex token;
};

struct ExtensionDefinition {
  static constexpr auto Kind = NodeKind::ExtensionDefinition.Define(
      {.category = NodeCategory::Decl,
       .bracketed_by = ExtensionDefinitionStart::Kind});

  ExtensionDefinitionStartId start;
  llvm::SmallVector<AnyDeclId> members;
  Lex::CloseCurlyBraceTokenIndex token;
};

// --- Inheritance clause ---
using InheritanceClauseStart =
    LeafNode<NodeKind::InheritanceClauseStart, Lex::ColonTokenIndex>;

// `: Type, Type`
struct InheritanceClause {
  static constexpr auto Kind = NodeKind::InheritanceClause.Define(
      {.bracketed_by = InheritanceClauseStart::Kind});

  InheritanceClauseStartId colon;
  CommaSeparatedList<AnyTypeExprId, PatternListCommaId> types;
  Lex::TokenIndex token;
};

// --- Typealias ---
using TypealiasIntroducer =
    LeafNode<NodeKind::TypealiasIntroducer, Lex::TypealiasKeywordTokenIndex>;

// `typealias Name = Type`
struct TypealiasDecl {
  static constexpr auto Kind = NodeKind::TypealiasDecl.Define(
      {.category = NodeCategory::Decl,
       .bracketed_by = TypealiasIntroducer::Kind});

  TypealiasIntroducerId introducer;
  IdentifierNameNotBeforeParamsId name;
  std::optional<GenericParameterClauseId> generic_params;
  Lex::EqualTokenIndex token;
  AnyTypeExprId type;
};

// ============================================================================
// Init/Deinit/Subscript
// ============================================================================

using InitIntroducer =
    LeafNode<NodeKind::InitIntroducer, Lex::InitKeywordTokenIndex>;

struct InitDefinitionStart {
  static constexpr auto Kind = NodeKind::InitDefinitionStart.Define(
      {.bracketed_by = InitIntroducer::Kind});

  InitIntroducerId introducer;
  std::optional<ExplicitParamListId> params;
  Lex::TokenIndex token;
};

struct InitDefinition {
  static constexpr auto Kind = NodeKind::InitDefinition.Define(
      {.category = NodeCategory::Decl,
       .bracketed_by = InitDefinitionStart::Kind});

  InitDefinitionStartId start;
  llvm::SmallVector<AnyStatementId> body;
  Lex::CloseCurlyBraceTokenIndex token;
};

using DeinitIntroducer =
    LeafNode<NodeKind::DeinitIntroducer, Lex::DeinitKeywordTokenIndex>;

struct DeinitDefinition {
  static constexpr auto Kind = NodeKind::DeinitDefinition.Define(
      {.category = NodeCategory::Decl,
       .bracketed_by = DeinitIntroducer::Kind});

  DeinitIntroducerId introducer;
  llvm::SmallVector<AnyStatementId> body;
  Lex::CloseCurlyBraceTokenIndex token;
};

using SubscriptIntroducer =
    LeafNode<NodeKind::SubscriptIntroducer, Lex::SubscriptKeywordTokenIndex>;

struct SubscriptDefinitionStart {
  static constexpr auto Kind = NodeKind::SubscriptDefinitionStart.Define(
      {.bracketed_by = SubscriptIntroducer::Kind});

  SubscriptIntroducerId introducer;
  std::optional<ExplicitParamListId> params;
  std::optional<ReturnTypeId> return_type;
  Lex::TokenIndex token;
};

struct SubscriptDefinition {
  static constexpr auto Kind = NodeKind::SubscriptDefinition.Define(
      {.category = NodeCategory::Decl,
       .bracketed_by = SubscriptDefinitionStart::Kind});

  SubscriptDefinitionStartId start;
  llvm::SmallVector<AnyStatementId> body;
  Lex::CloseCurlyBraceTokenIndex token;
};

// ============================================================================
// Accessors
// ============================================================================

using AccessorBlockStart =
    LeafNode<NodeKind::AccessorBlockStart, Lex::OpenCurlyBraceTokenIndex>;

struct AccessorBlock {
  static constexpr auto Kind = NodeKind::AccessorBlock.Define(
      {.bracketed_by = AccessorBlockStart::Kind});

  AccessorBlockStartId left_brace;
  llvm::SmallVector<NodeId> accessors;
  Lex::CloseCurlyBraceTokenIndex token;
};

struct GetAccessor {
  static constexpr auto Kind = NodeKind::GetAccessor.Define(
      {.child_count = 0});

  Lex::TokenIndex token;
};

struct SetAccessor {
  static constexpr auto Kind = NodeKind::SetAccessor.Define(
      {.child_count = 0});

  Lex::TokenIndex token;
};

struct WillSetAccessor {
  static constexpr auto Kind = NodeKind::WillSetAccessor.Define(
      {.child_count = 0});

  Lex::TokenIndex token;
};

struct DidSetAccessor {
  static constexpr auto Kind = NodeKind::DidSetAccessor.Define(
      {.child_count = 0});

  Lex::TokenIndex token;
};

// ============================================================================
// Statement nodes (Swift-style: no semicolons)
// ============================================================================

using CodeBlockStart =
    LeafNode<NodeKind::CodeBlockStart, Lex::OpenCurlyBraceTokenIndex>;

// `{ statement; statement; ... }`
struct CodeBlock {
  static constexpr auto Kind =
      NodeKind::CodeBlock.Define({.bracketed_by = CodeBlockStart::Kind});

  CodeBlockStartId left_brace;
  llvm::SmallVector<AnyStatementId> statements;
  Lex::CloseCurlyBraceTokenIndex token;
};

// An expression statement (no semicolon required in Swift).
struct ExprStatement {
  static constexpr auto Kind = NodeKind::ExprStatement.Define(
      {.category = NodeCategory::Statement, .child_count = 0});

  AnyExprId expr;
  Lex::TokenIndex token;
};

using BreakStatementStart =
    LeafNode<NodeKind::BreakStatementStart, Lex::BreakKeywordTokenIndex>;

// `break` or `break label`
struct BreakStatement {
  static constexpr auto Kind = NodeKind::BreakStatement.Define(
      {.category = NodeCategory::Statement,
       .bracketed_by = BreakStatementStart::Kind});

  BreakStatementStartId introducer;
  Lex::BreakKeywordTokenIndex token;
};

using ContinueStatementStart =
    LeafNode<NodeKind::ContinueStatementStart, Lex::ContinueKeywordTokenIndex>;

// `continue` or `continue label`
struct ContinueStatement {
  static constexpr auto Kind = NodeKind::ContinueStatement.Define(
      {.category = NodeCategory::Statement,
       .bracketed_by = ContinueStatementStart::Kind});

  ContinueStatementStartId introducer;
  Lex::ContinueKeywordTokenIndex token;
};

using ThrowStatementStart =
    LeafNode<NodeKind::ThrowStatementStart, Lex::ThrowKeywordTokenIndex>;

// `throw expr`
struct ThrowStatement {
  static constexpr auto Kind = NodeKind::ThrowStatement.Define(
      {.category = NodeCategory::Statement,
       .bracketed_by = ThrowStatementStart::Kind});

  ThrowStatementStartId introducer;
  AnyExprId expr;
  Lex::ThrowKeywordTokenIndex token;
};

// `fallthrough`
using FallthroughStatement =
    LeafNode<NodeKind::FallthroughStatement, Lex::FallthroughKeywordTokenIndex,
             NodeCategory::Statement>;

using ReturnStatementStart =
    LeafNode<NodeKind::ReturnStatementStart, Lex::ReturnKeywordTokenIndex>;

// `return expr`
struct ReturnStatement {
  static constexpr auto Kind = NodeKind::ReturnStatement.Define(
      {.category = NodeCategory::Statement,
       .bracketed_by = ReturnStatementStart::Kind});

  ReturnStatementStartId introducer;
  std::optional<AnyExprId> expr;
  Lex::ReturnKeywordTokenIndex token;
};

// ============================================================================
// Control flow (Swift-style: bare conditions, no parens)
// ============================================================================

// The condition of an `if` statement: bare expression (no parens in Swift).
struct IfCondition {
  static constexpr auto Kind = NodeKind::IfCondition.Define(
      {.child_count = 0});

  AnyExprId condition;
  Lex::TokenIndex token;
};

using IfStatementElse =
    LeafNode<NodeKind::IfStatementElse, Lex::ElseKeywordTokenIndex>;

// `if expr { ... } else { ... }`
struct IfStatement {
  static constexpr auto Kind = NodeKind::IfStatement.Define(
      {.category = NodeCategory::Statement,
       .bracketed_by = IfCondition::Kind});

  Lex::IfKeywordTokenIndex token;
  IfConditionId head;
  CodeBlockId then;

  struct Else {
    IfStatementElseId else_token;
    NodeIdOneOf<CodeBlock, IfStatement> body;
  };
  std::optional<Else> else_clause;
};

// Guard condition
struct GuardCondition {
  static constexpr auto Kind = NodeKind::GuardCondition.Define(
      {.child_count = 0});

  AnyExprId condition;
  Lex::TokenIndex token;
};

using GuardStatementElse =
    LeafNode<NodeKind::GuardStatementElse, Lex::ElseKeywordTokenIndex>;

// `guard condition else { ... }`
struct GuardStatement {
  static constexpr auto Kind = NodeKind::GuardStatement.Define(
      {.category = NodeCategory::Statement,
       .bracketed_by = GuardCondition::Kind});

  Lex::GuardKeywordTokenIndex token;
  GuardConditionId head;
  GuardStatementElseId else_token;
  CodeBlockId body;
};

// While condition (bare expression)
struct WhileCondition {
  static constexpr auto Kind = NodeKind::WhileCondition.Define(
      {.child_count = 0});

  AnyExprId condition;
  Lex::TokenIndex token;
};

// `while condition { ... }`
struct WhileStatement {
  static constexpr auto Kind =
      NodeKind::WhileStatement.Define({.category = NodeCategory::Statement,
                                       .bracketed_by = WhileCondition::Kind,
                                       .child_count = 2});

  Lex::WhileKeywordTokenIndex token;
  WhileConditionId head;
  CodeBlockId body;
};

// `for` introducer
using ForInIntroducer =
    LeafNode<NodeKind::ForInIntroducer, Lex::ForKeywordTokenIndex>;

// `for pattern in expr { ... }`
struct ForInStatement {
  static constexpr auto Kind = NodeKind::ForInStatement.Define(
      {.category = NodeCategory::Statement,
       .bracketed_by = ForInIntroducer::Kind});

  ForInIntroducerId introducer;
  AnyPatternId pattern;
  AnyExprId sequence;
  CodeBlockId body;
  Lex::TokenIndex token;
};

// `repeat { ... } while expr`
struct RepeatWhileStatement {
  static constexpr auto Kind = NodeKind::RepeatWhileStatement.Define(
      {.category = NodeCategory::Statement, .child_count = 2});

  Lex::RepeatKeywordTokenIndex token;
  CodeBlockId body;
  AnyExprId condition;
};

// --- Switch ---
using SwitchIntroducer =
    LeafNode<NodeKind::SwitchIntroducer, Lex::SwitchKeywordTokenIndex>;

struct SwitchCaseLabel {
  static constexpr auto Kind = NodeKind::SwitchCaseLabel.Define(
      {.child_count = 1});

  Lex::CaseKeywordTokenIndex token;
  AnyPatternId pattern;
};

struct SwitchDefaultLabel {
  static constexpr auto Kind = NodeKind::SwitchDefaultLabel.Define(
      {.child_count = 0});

  Lex::DefaultKeywordTokenIndex token;
};

struct SwitchCaseBody {
  static constexpr auto Kind = NodeKind::SwitchCaseBody.Define(
      {.bracketed_by = SwitchCaseLabel::Kind});

  NodeIdOneOf<SwitchCaseLabel, SwitchDefaultLabel> label;
  llvm::SmallVector<AnyStatementId> statements;
  Lex::TokenIndex token;
};

struct SwitchStatement {
  static constexpr auto Kind = NodeKind::SwitchStatement.Define(
      {.category = NodeCategory::Statement,
       .bracketed_by = SwitchIntroducer::Kind});

  SwitchIntroducerId introducer;
  AnyExprId subject;
  llvm::SmallVector<SwitchCaseBodyId> cases;
  Lex::CloseCurlyBraceTokenIndex token;
};

// --- Do/Catch ---
using DoIntroducer =
    LeafNode<NodeKind::DoIntroducer, Lex::DoKeywordTokenIndex>;

struct DoCatchClause {
  static constexpr auto Kind = NodeKind::DoCatchClause.Define(
      {.child_count = 1});

  Lex::CatchKeywordTokenIndex token;
  CodeBlockId body;
};

struct DoStatement {
  static constexpr auto Kind = NodeKind::DoStatement.Define(
      {.category = NodeCategory::Statement,
       .bracketed_by = DoIntroducer::Kind});

  DoIntroducerId introducer;
  CodeBlockId body;
  llvm::SmallVector<DoCatchClauseId> catch_clauses;
  Lex::TokenIndex token;
};

// --- Defer ---
using DeferStatementStart =
    LeafNode<NodeKind::DeferStatementStart, Lex::DeferKeywordTokenIndex>;

struct DeferStatement {
  static constexpr auto Kind = NodeKind::DeferStatement.Define(
      {.category = NodeCategory::Statement,
       .bracketed_by = DeferStatementStart::Kind});

  DeferStatementStartId introducer;
  CodeBlockId body;
  Lex::DeferKeywordTokenIndex token;
};

// ============================================================================
// Condition lists
// ============================================================================

struct ConditionListItem {
  static constexpr auto Kind = NodeKind::ConditionListItem.Define(
      {.child_count = 0});

  AnyExprId condition;
  Lex::TokenIndex token;
};

struct OptionalBindingCondition {
  static constexpr auto Kind = NodeKind::OptionalBindingCondition.Define(
      {.child_count = 1});

  Lex::TokenIndex token;
  AnyPatternId pattern;
};

// ============================================================================
// Expression nodes
// ============================================================================

using ParenExprStart =
    LeafNode<NodeKind::ParenExprStart, Lex::OpenParenTokenIndex>;

// `(expr)`
struct ParenExpr {
  static constexpr auto Kind = NodeKind::ParenExpr.Define(
      {.category = NodeCategory::Expr | NodeCategory::MemberExpr,
       .bracketed_by = ParenExprStart::Kind,
       .child_count = 2});

  ParenExprStartId start;
  AnyExprId expr;
  Lex::CloseParenTokenIndex token;
};

// `callee(`
struct CallExprStart {
  static constexpr auto Kind =
      NodeKind::CallExprStart.Define({.child_count = 1});

  AnyExprId callee;
  Lex::OpenParenTokenIndex token;
};

// A labeled call argument: `label: expr` or just `expr`
struct CallArgument {
  static constexpr auto Kind = NodeKind::CallArgument.Define(
      {.child_count = 0});

  std::optional<ArgumentLabelId> label;
  AnyExprId expr;
  Lex::TokenIndex token;
};

// `label:`
struct ArgumentLabel {
  static constexpr auto Kind = NodeKind::ArgumentLabel.Define(
      {.child_count = 0});

  Lex::TokenIndex token;
};

// `callee(args...)`
struct CallExpr {
  static constexpr auto Kind = NodeKind::CallExpr.Define(
      {.category = NodeCategory::Expr, .bracketed_by = CallExprStart::Kind});

  CallExprStartId start;
  CommaSeparatedList<AnyExprId, PatternListCommaId> arguments;
  Lex::CloseParenTokenIndex token;
};

// `expr.member`
struct MemberAccessExpr {
  static constexpr auto Kind = NodeKind::MemberAccessExpr.Define(
      {.category = NodeCategory::Expr, .child_count = 2});

  AnyExprId lhs;
  Lex::PeriodTokenIndex token;
  AnyMemberAccessId rhs;
};

// `expr[`
struct SubscriptExprStart {
  static constexpr auto Kind =
      NodeKind::SubscriptExprStart.Define({.child_count = 1});

  AnyExprId base;
  Lex::OpenSquareBracketTokenIndex token;
};

// `expr[args]`
struct SubscriptExpr {
  static constexpr auto Kind = NodeKind::SubscriptExpr.Define(
      {.category = NodeCategory::Expr,
       .bracketed_by = SubscriptExprStart::Kind});

  SubscriptExprStartId start;
  CommaSeparatedList<AnyExprId, PatternListCommaId> arguments;
  Lex::CloseSquareBracketTokenIndex token;
};

// Literals
using IntLiteral = LeafNode<NodeKind::IntLiteral, Lex::IntegerLiteralTokenIndex,
                            NodeCategory::Expr | NodeCategory::IntConst>;

using FloatingLiteral =
    LeafNode<NodeKind::FloatingLiteral, Lex::FloatingLiteralTokenIndex,
             NodeCategory::Expr>;

using NilLiteral =
    LeafNode<NodeKind::NilLiteral, Lex::NilKeywordTokenIndex,
             NodeCategory::Expr>;

// Generic operator expression nodes (replacing per-operator templates)

// `-expr`, `!expr`
struct PrefixOperatorExpr {
  static constexpr auto Kind = NodeKind::PrefixOperatorExpr.Define(
      {.category = NodeCategory::Expr, .child_count = 1});

  Lex::TokenIndex token;
  AnyExprId operand;
};

// `expr + expr`
struct InfixOperatorExpr {
  static constexpr auto Kind = NodeKind::InfixOperatorExpr.Define(
      {.category = NodeCategory::Expr, .child_count = 2});

  AnyExprId lhs;
  Lex::TokenIndex token;
  AnyExprId rhs;
};

// `expr!`, `expr?`
struct PostfixOperatorExpr {
  static constexpr auto Kind = NodeKind::PostfixOperatorExpr.Define(
      {.category = NodeCategory::Expr, .child_count = 1});

  AnyExprId operand;
  Lex::TokenIndex token;
};

// `lhs = rhs`
struct AssignmentExpr {
  static constexpr auto Kind = NodeKind::AssignmentExpr.Define(
      {.category = NodeCategory::Expr, .child_count = 2});

  AnyExprId lhs;
  Lex::EqualTokenIndex token;
  AnyExprId rhs;
};

// `condition ? then : else`
struct TernaryExpr {
  static constexpr auto Kind = NodeKind::TernaryExpr.Define(
      {.category = NodeCategory::Expr, .child_count = 3});

  AnyExprId condition;
  Lex::QuestionTokenIndex token;
  AnyExprId then_expr;
  AnyExprId else_expr;
};

// `expr as Type`
struct AsExpr {
  static constexpr auto Kind = NodeKind::AsExpr.Define(
      {.category = NodeCategory::Expr, .child_count = 1});

  AnyExprId expr;
  Lex::AsKeywordTokenIndex token;
  AnyTypeExprId type;
};

// `expr as! Type`
struct AsExclaimExpr {
  static constexpr auto Kind = NodeKind::AsExclaimExpr.Define(
      {.category = NodeCategory::Expr, .child_count = 1});

  AnyExprId expr;
  Lex::TokenIndex token;
  AnyTypeExprId type;
};

// `expr as? Type`
struct AsQuestionExpr {
  static constexpr auto Kind = NodeKind::AsQuestionExpr.Define(
      {.category = NodeCategory::Expr, .child_count = 1});

  AnyExprId expr;
  Lex::TokenIndex token;
  AnyTypeExprId type;
};

// `expr is Type`
struct IsExpr {
  static constexpr auto Kind = NodeKind::IsExpr.Define(
      {.category = NodeCategory::Expr, .child_count = 1});

  AnyExprId expr;
  Lex::IsKeywordTokenIndex token;
  AnyTypeExprId type;
};

// `try expr`
struct TryExpr {
  static constexpr auto Kind = NodeKind::TryExpr.Define(
      {.category = NodeCategory::Expr, .child_count = 1});

  Lex::TryKeywordTokenIndex token;
  AnyExprId expr;
};

// `try! expr`
struct TryExclaimExpr {
  static constexpr auto Kind = NodeKind::TryExclaimExpr.Define(
      {.category = NodeCategory::Expr, .child_count = 1});

  Lex::TokenIndex token;
  AnyExprId expr;
};

// `try? expr`
struct TryQuestionExpr {
  static constexpr auto Kind = NodeKind::TryQuestionExpr.Define(
      {.category = NodeCategory::Expr, .child_count = 1});

  Lex::TokenIndex token;
  AnyExprId expr;
};

// --- Closures ---
using ClosureExprStart =
    LeafNode<NodeKind::ClosureExprStart, Lex::OpenCurlyBraceTokenIndex>;

struct ClosureParam {
  static constexpr auto Kind = NodeKind::ClosureParam.Define(
      {.child_count = 0});

  Lex::TokenIndex token;
};

struct ClosureSignature {
  static constexpr auto Kind = NodeKind::ClosureSignature.Define(
      {.child_count = 0});

  Lex::TokenIndex token;
};

// `{ params in body }` or `{ $0 + $1 }`
struct ClosureExpr {
  static constexpr auto Kind = NodeKind::ClosureExpr.Define(
      {.category = NodeCategory::Expr,
       .bracketed_by = ClosureExprStart::Kind});

  ClosureExprStartId start;
  std::optional<ClosureSignatureId> signature;
  llvm::SmallVector<AnyStatementId> body;
  Lex::CloseCurlyBraceTokenIndex token;
};

// --- Tuples ---
using TupleExprStart =
    LeafNode<NodeKind::TupleExprStart, Lex::OpenParenTokenIndex>;

struct TupleElement {
  static constexpr auto Kind = NodeKind::TupleElement.Define(
      {.child_count = 0});

  Lex::TokenIndex token;
};

struct TupleExpr {
  static constexpr auto Kind = NodeKind::TupleExpr.Define(
      {.category = NodeCategory::Expr,
       .bracketed_by = ParenExprStart::Kind});

  TupleExprStartId start;
  CommaSeparatedList<AnyExprId, PatternListCommaId> elements;
  Lex::CloseParenTokenIndex token;
};

// --- Array literals ---
using ArrayExprStart =
    LeafNode<NodeKind::ArrayExprStart, Lex::OpenSquareBracketTokenIndex>;

struct ArrayExpr {
  static constexpr auto Kind = NodeKind::ArrayExpr.Define(
      {.category = NodeCategory::Expr,
       .bracketed_by = ArrayExprStart::Kind});

  ArrayExprStartId start;
  CommaSeparatedList<AnyExprId, PatternListCommaId> elements;
  Lex::CloseSquareBracketTokenIndex token;
};

// --- Dictionary literals ---
using DictionaryExprStart =
    LeafNode<NodeKind::DictionaryExprStart, Lex::OpenSquareBracketTokenIndex>;

struct DictionaryExprEntry {
  static constexpr auto Kind = NodeKind::DictionaryExprEntry.Define(
      {.child_count = 2});

  AnyExprId key;
  Lex::ColonTokenIndex token;
  AnyExprId value;
};

struct DictionaryExpr {
  static constexpr auto Kind = NodeKind::DictionaryExpr.Define(
      {.category = NodeCategory::Expr,
       .bracketed_by = DictionaryExprStart::Kind});

  DictionaryExprStartId start;
  CommaSeparatedList<DictionaryExprEntryId, PatternListCommaId> entries;
  Lex::CloseSquareBracketTokenIndex token;
};

// String interpolation
struct StringInterpolationExpr {
  static constexpr auto Kind = NodeKind::StringInterpolationExpr.Define(
      {.category = NodeCategory::Expr, .child_count = 0});

  Lex::TokenIndex token;
};

// M40: `&varName` — address-of prefix expression.
struct AddressOfExpr {
  static constexpr auto Kind = NodeKind::AddressOfExpr.Define(
      {.category = NodeCategory::Expr, .child_count = 1});

  Lex::TokenIndex token;
  AnyExprId operand;
};

// Key path
struct KeyPathComponent {
  static constexpr auto Kind = NodeKind::KeyPathComponent.Define(
      {.child_count = 0});

  Lex::TokenIndex token;
};

struct KeyPathExpr {
  static constexpr auto Kind = NodeKind::KeyPathExpr.Define(
      {.category = NodeCategory::Expr, .child_count = 0});

  Lex::BackslashTokenIndex token;
};

// ============================================================================
// Type expression nodes
// ============================================================================

// `: Type`
struct TypeAnnotation {
  static constexpr auto Kind = NodeKind::TypeAnnotation.Define(
      {.child_count = 1});

  Lex::ColonTokenIndex token;
  AnyTypeExprId type;
};

// M40: `inout` keyword marker inside a type annotation.
using InoutMarker =
    LeafNode<NodeKind::InoutMarker, Lex::TokenIndex, NodeCategory::None>;

// `Int`, `String`, `MyType`
using IdentifierType =
    LeafNode<NodeKind::IdentifierType, Lex::TokenIndex,
             NodeCategory::Type>;

// `Type?`
struct OptionalType {
  static constexpr auto Kind = NodeKind::OptionalType.Define(
      {.category = NodeCategory::Type, .child_count = 1});

  AnyTypeExprId base;
  Lex::QuestionTokenIndex token;
};

// `Type!`
struct ImplicitlyUnwrappedOptionalType {
  static constexpr auto Kind = NodeKind::ImplicitlyUnwrappedOptionalType.Define(
      {.category = NodeCategory::Type, .child_count = 1});

  AnyTypeExprId base;
  Lex::ExclaimTokenIndex token;
};

// `[Type]`
struct ArrayType {
  static constexpr auto Kind = NodeKind::ArrayType.Define(
      {.category = NodeCategory::Type, .child_count = 2});

  Lex::OpenSquareBracketTokenIndex token;
  AnyTypeExprId element;
  Lex::CloseSquareBracketTokenIndex close;
};

// `[Key: Value]`
struct DictionaryType {
  static constexpr auto Kind = NodeKind::DictionaryType.Define(
      {.category = NodeCategory::Type, .child_count = 3});

  Lex::OpenSquareBracketTokenIndex token;
  AnyTypeExprId key;
  AnyTypeExprId value;
  Lex::CloseSquareBracketTokenIndex close;
};

// `(Type, Type)`
struct TupleType {
  static constexpr auto Kind = NodeKind::TupleType.Define(
      {.category = NodeCategory::Type, .child_count = 0});

  Lex::OpenParenTokenIndex token;
};

// `(Type) -> Type`
struct FunctionType {
  static constexpr auto Kind = NodeKind::FunctionType.Define(
      {.category = NodeCategory::Type, .child_count = 2});

  AnyTypeExprId param;
  Lex::MinusGreaterTokenIndex token;
  AnyTypeExprId result;
};

// `Type & Type`
struct ProtocolCompositionType {
  static constexpr auto Kind = NodeKind::ProtocolCompositionType.Define(
      {.category = NodeCategory::Type, .child_count = 2});

  AnyTypeExprId lhs;
  Lex::AmpTokenIndex token;
  AnyTypeExprId rhs;
};

// `Type.Member`
struct MemberType {
  static constexpr auto Kind = NodeKind::MemberType.Define(
      {.category = NodeCategory::Type, .child_count = 2});

  AnyTypeExprId base;
  Lex::PeriodTokenIndex token;
  AnyTypeExprId member;
};

// `Type.Type` or `Type.Protocol`
struct MetatypeType {
  static constexpr auto Kind = NodeKind::MetatypeType.Define(
      {.category = NodeCategory::Type, .child_count = 1});

  AnyTypeExprId base;
  Lex::PeriodTokenIndex token;
};

// ============================================================================
// Generics
// ============================================================================

using GenericParameterClauseStart =
    LeafNode<NodeKind::GenericParameterClauseStart, Lex::TokenIndex>;

// `T: Equatable` or just `T`
struct GenericParameter {
  static constexpr auto Kind = NodeKind::GenericParameter.Define(
      {.child_count = 0});

  Lex::TokenIndex token;
};

// `<T, U: Equatable>`
struct GenericParameterClause {
  static constexpr auto Kind = NodeKind::GenericParameterClause.Define(
      {.bracketed_by = GenericParameterClauseStart::Kind});

  GenericParameterClauseStartId open;
  CommaSeparatedList<GenericParameterId, PatternListCommaId> params;
  Lex::TokenIndex token;
};

using GenericArgumentClauseStart =
    LeafNode<NodeKind::GenericArgumentClauseStart, Lex::TokenIndex>;

// `<Int, String>`
struct GenericArgumentClause {
  static constexpr auto Kind = NodeKind::GenericArgumentClause.Define(
      {.bracketed_by = GenericArgumentClauseStart::Kind});

  GenericArgumentClauseStartId open;
  CommaSeparatedList<AnyTypeExprId, PatternListCommaId> args;
  Lex::TokenIndex token;
};

// `where T == Int`
struct SameTypeRequirement {
  static constexpr auto Kind = NodeKind::SameTypeRequirement.Define(
      {.category = NodeCategory::Requirement, .child_count = 2});

  AnyTypeExprId lhs;
  Lex::TokenIndex token;
  AnyTypeExprId rhs;
};

// `where T: Equatable`
struct ConformanceRequirement {
  static constexpr auto Kind = NodeKind::ConformanceRequirement.Define(
      {.category = NodeCategory::Requirement, .child_count = 2});

  AnyTypeExprId type;
  Lex::ColonTokenIndex token;
  AnyTypeExprId constraint;
};

// `where ...`
struct GenericWhereClause {
  static constexpr auto Kind = NodeKind::GenericWhereClause.Define(
      {.child_count = 0});

  Lex::WhereKeywordTokenIndex token;
};

// ============================================================================
// Patterns
// ============================================================================

// `_`
using WildcardPattern =
    LeafNode<NodeKind::WildcardPattern, Lex::UnderscoreKeywordTokenIndex,
             NodeCategory::Pattern>;

// `name`
using IdentifierPattern =
    LeafNode<NodeKind::IdentifierPattern, Lex::IdentifierTokenIndex,
             NodeCategory::Pattern>;

struct TuplePatternElement {
  static constexpr auto Kind = NodeKind::TuplePatternElement.Define(
      {.child_count = 0});

  Lex::TokenIndex token;
};

// `(x, y)`
struct TuplePattern {
  static constexpr auto Kind = NodeKind::TuplePattern.Define(
      {.category = NodeCategory::Pattern, .child_count = 0});

  Lex::OpenParenTokenIndex token;
};

// `.case(associated)`
struct EnumCasePattern {
  static constexpr auto Kind = NodeKind::EnumCasePattern.Define(
      {.category = NodeCategory::Pattern, .child_count = 0});

  Lex::PeriodTokenIndex token;
};

// `x?`
struct OptionalPattern {
  static constexpr auto Kind = NodeKind::OptionalPattern.Define(
      {.category = NodeCategory::Pattern, .child_count = 1});

  AnyPatternId base;
  Lex::QuestionTokenIndex token;
};

// `is Type`
struct IsPattern {
  static constexpr auto Kind = NodeKind::IsPattern.Define(
      {.category = NodeCategory::Pattern, .child_count = 1});

  Lex::IsKeywordTokenIndex token;
  AnyTypeExprId type;
};

// An expression used as a pattern.
struct ExprPattern {
  static constexpr auto Kind = NodeKind::ExprPattern.Define(
      {.category = NodeCategory::Pattern, .child_count = 0});

  AnyExprId expr;
  Lex::TokenIndex token;
};

// ============================================================================
// Attributes and modifiers
// ============================================================================

using AttributeStart =
    LeafNode<NodeKind::AttributeStart, Lex::AtTokenIndex>;

// `@attribute` or `@attribute(args)`
struct Attribute {
  static constexpr auto Kind = NodeKind::Attribute.Define(
      {.category = NodeCategory::Modifier,
       .bracketed_by = AttributeStart::Kind});

  AttributeStartId at_sign;
  Lex::TokenIndex token;
};

// `public`, `private`, etc.
struct AccessModifier {
  static constexpr auto Kind = NodeKind::AccessModifier.Define(
      {.category = NodeCategory::Modifier, .child_count = 0});

  Lex::TokenIndex token;
};

// `static`
using StaticModifier =
    LeafNode<NodeKind::StaticModifier, Lex::StaticKeywordTokenIndex,
             NodeCategory::Modifier>;

// `mutating` (contextual keyword — emitted as Identifier token)
using MutatingModifier =
    LeafNode<NodeKind::MutatingModifier, Lex::IdentifierTokenIndex,
             NodeCategory::Modifier>;

// ============================================================================
// Conditional compilation
// ============================================================================

using PoundIfStart =
    LeafNode<NodeKind::PoundIfStart, Lex::PoundIfTokenIndex>;

struct PoundIfDecl {
  static constexpr auto Kind = NodeKind::PoundIfDecl.Define(
      {.category = NodeCategory::Decl,
       .bracketed_by = PoundIfStart::Kind});

  PoundIfStartId start;
  llvm::SmallVector<AnyDeclId> members;
  Lex::TokenIndex token;
};

struct PoundElseDecl {
  static constexpr auto Kind = NodeKind::PoundElseDecl.Define(
      {.category = NodeCategory::Decl, .child_count = 0});

  Lex::PoundElseTokenIndex token;
};

struct PoundElseifDecl {
  static constexpr auto Kind = NodeKind::PoundElseifDecl.Define(
      {.category = NodeCategory::Decl, .child_count = 0});

  Lex::PoundElseifTokenIndex token;
};

struct PoundEndifDecl {
  static constexpr auto Kind = NodeKind::PoundEndifDecl.Define(
      {.category = NodeCategory::Decl, .child_count = 0});

  Lex::PoundEndifTokenIndex token;
};

// ============================================================================
// Literals from node_kind.def x-macros
// ============================================================================

#define TINYSWIFT_PARSE_NODE_KIND(Name)
#define TINYSWIFT_PARSE_NODE_KIND_TOKEN_LITERAL(Name, LexTokenKind)       \
  using Name = LeafNode<NodeKind::Name, Lex::LexTokenKind##TokenIndex, \
                        NodeCategory::Expr>;
#define TINYSWIFT_PARSE_NODE_KIND_TOKEN_MODIFIER(Name)             \
  using Name##Modifier =                                        \
      LeafNode<NodeKind::Name##Modifier, Lex::Name##TokenIndex, \
               NodeCategory::Modifier>;
#include "toolchain/parse/node_kind.def"

// ---------------------------------------------------------------------------

// A complete source file.
struct File {
  FileStartId start;
  llvm::SmallVector<AnyDeclId> decls;
  FileEndId end;
};

// Define `Foo` as the node type for the ID type `FooId`.
#define TINYSWIFT_PARSE_NODE_KIND(KindName) \
  template <>                            \
  struct NodeForId<KindName##Id> {       \
    using TypedNode = KindName;          \
  };
#include "toolchain/parse/node_kind.def"

}  // namespace TinySwift::Parse

#endif  // TINYSWIFT_TOOLCHAIN_PARSE_TYPED_NODES_H_
