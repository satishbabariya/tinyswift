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

// Error nodes
// -----------

// An invalid parse. Used to balance the parse tree.
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

// A placeholder node to be replaced; it will never exist in a valid parse tree.
using Placeholder = LeafNode<NodeKind::Placeholder, Lex::TokenIndex>;

// File nodes
// ----------

using FileStart = LeafNode<NodeKind::FileStart, Lex::FileStartTokenIndex>;
using FileEnd = LeafNode<NodeKind::FileEnd, Lex::FileEndTokenIndex>;

// General-purpose nodes
// ---------------------

// An empty declaration, such as `;`.
using EmptyDecl =
    LeafNode<NodeKind::EmptyDecl, Lex::SemiTokenIndex, NodeCategory::Decl>;

// A name in a non-expression context, known to be followed by parameters.
using IdentifierNameBeforeParams =
    LeafNode<NodeKind::IdentifierNameBeforeParams, Lex::IdentifierTokenIndex,
             NodeCategory::MemberName | NodeCategory::NonExprName>;

// A name in a non-expression context, known to not be followed by parameters.
using IdentifierNameNotBeforeParams =
    LeafNode<NodeKind::IdentifierNameNotBeforeParams, Lex::IdentifierTokenIndex,
             NodeCategory::MemberName | NodeCategory::NonExprName>;

// A name in an expression context.
using IdentifierNameExpr =
    LeafNode<NodeKind::IdentifierNameExpr, Lex::IdentifierTokenIndex,
             NodeCategory::Expr>;

// Import nodes
// ------------

using ImportIntroducer =
    LeafNode<NodeKind::ImportIntroducer, Lex::ImportTokenIndex>;

// An import declaration: `import ...;`
// TODO: Add import source fields (package name, library specifier, etc.)
struct ImportDecl {
  static constexpr auto Kind = NodeKind::ImportDecl.Define(
      {.category = NodeCategory::Decl,
       .bracketed_by = ImportIntroducer::Kind,
       .child_count = 1});

  ImportIntroducerId introducer;
  Lex::SemiTokenIndex token;
};

// Function nodes
// --------------

using FunctionIntroducer =
    LeafNode<NodeKind::FunctionIntroducer, Lex::FnTokenIndex>;

// A return type: `-> i32`.
struct ReturnType {
  static constexpr auto Kind = NodeKind::ReturnType.Define({.child_count = 1});

  Lex::MinusGreaterTokenIndex token;
  AnyExprId type;
};

// A function signature: `fn F(params) -> i32`.
template <const NodeKind& KindT, typename TokenKind,
          NodeCategory::RawEnumType Category>
struct FunctionSignature {
  static constexpr auto Kind = KindT.Define(
      {.category = Category, .bracketed_by = FunctionIntroducer::Kind});

  FunctionIntroducerId introducer;
  AnyNonExprNameId name;
  std::optional<ExplicitParamListId> params;
  std::optional<ReturnTypeId> return_type;
  TokenKind token;
};

// `fn F() -> i32;`
using FunctionDecl = FunctionSignature<NodeKind::FunctionDecl,
                                       Lex::SemiTokenIndex, NodeCategory::Decl>;
// `fn F() -> i32 {`
using FunctionDefinitionStart =
    FunctionSignature<NodeKind::FunctionDefinitionStart, Lex::TokenIndex,
                      NodeCategory::None>;

// A function definition: `fn F() -> i32 { ... }`.
struct FunctionDefinition {
  static constexpr auto Kind = NodeKind::FunctionDefinition.Define(
      {.category = NodeCategory::Decl,
       .bracketed_by = FunctionDefinitionStart::Kind});

  FunctionDefinitionStartId signature;
  llvm::SmallVector<AnyStatementId> body;
  Lex::CloseCurlyBraceTokenIndex token;
};

// Parameter nodes
// ---------------

using ExplicitParamListStart =
    LeafNode<NodeKind::ExplicitParamListStart, Lex::OpenParenTokenIndex>;

using PatternListComma =
    LeafNode<NodeKind::PatternListComma, Lex::CommaTokenIndex>;

// An explicit parameter list: `(a: i32, b: i32)`.
struct ExplicitParamList {
  static constexpr auto Kind = NodeKind::ExplicitParamList.Define(
      {.bracketed_by = ExplicitParamListStart::Kind});

  ExplicitParamListStartId left_paren;
  CommaSeparatedList<AnyPatternId, PatternListCommaId> params;
  Lex::CloseParenTokenIndex token;
};

// Let nodes
// ---------

using LetIntroducer = LeafNode<NodeKind::LetIntroducer, Lex::LetTokenIndex>;
using LetInitializer = LeafNode<NodeKind::LetInitializer, Lex::EqualTokenIndex>;

// A binding pattern: `name: Type`.
struct LetBindingPattern {
  static constexpr auto Kind = NodeKind::LetBindingPattern.Define(
      {.category = NodeCategory::Pattern, .child_count = 2});

  IdentifierNameNotBeforeParamsId name;
  Lex::ColonTokenIndex token;
  AnyExprId type;
};

// A `let` declaration: `let a: i32 = 5;`.
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
  Lex::SemiTokenIndex token;
};

// Var nodes
// ---------

using VariableIntroducer =
    LeafNode<NodeKind::VariableIntroducer, Lex::VarTokenIndex>;
using VariableInitializer =
    LeafNode<NodeKind::VariableInitializer, Lex::EqualTokenIndex>;

// A binding pattern inside a `var`: `name: Type`.
struct VarBindingPattern {
  static constexpr auto Kind = NodeKind::VarBindingPattern.Define(
      {.category = NodeCategory::Pattern, .child_count = 2});

  IdentifierNameNotBeforeParamsId name;
  Lex::ColonTokenIndex token;
  AnyExprId type;
};

// A `var` pattern.
struct VariablePattern {
  static constexpr auto Kind = NodeKind::VariablePattern.Define(
      {.category = NodeCategory::Pattern, .child_count = 1});

  Lex::VarTokenIndex token;
  AnyPatternId inner;
};

// A `var` declaration: `var a: i32;` or `var a: i32 = 5;`.
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
  Lex::SemiTokenIndex token;
};

// Statement nodes
// ---------------

using CodeBlockStart =
    LeafNode<NodeKind::CodeBlockStart, Lex::OpenCurlyBraceTokenIndex>;

// A code block: `{ statement; statement; ... }`.
struct CodeBlock {
  static constexpr auto Kind =
      NodeKind::CodeBlock.Define({.bracketed_by = CodeBlockStart::Kind});

  CodeBlockStartId left_brace;
  llvm::SmallVector<AnyStatementId> statements;
  Lex::CloseCurlyBraceTokenIndex token;
};

// An expression statement: `F(x);`.
struct ExprStatement {
  static constexpr auto Kind = NodeKind::ExprStatement.Define(
      {.category = NodeCategory::Statement, .child_count = 1});

  AnyExprId expr;
  Lex::SemiTokenIndex token;
};

using BreakStatementStart =
    LeafNode<NodeKind::BreakStatementStart, Lex::BreakTokenIndex>;

// A break statement: `break;`.
struct BreakStatement {
  static constexpr auto Kind = NodeKind::BreakStatement.Define(
      {.category = NodeCategory::Statement,
       .bracketed_by = BreakStatementStart::Kind,
       .child_count = 1});

  BreakStatementStartId introducer;
  Lex::SemiTokenIndex token;
};

using ContinueStatementStart =
    LeafNode<NodeKind::ContinueStatementStart, Lex::ContinueTokenIndex>;

// A continue statement: `continue;`.
struct ContinueStatement {
  static constexpr auto Kind = NodeKind::ContinueStatement.Define(
      {.category = NodeCategory::Statement,
       .bracketed_by = ContinueStatementStart::Kind,
       .child_count = 1});

  ContinueStatementStartId introducer;
  Lex::SemiTokenIndex token;
};

using ReturnStatementStart =
    LeafNode<NodeKind::ReturnStatementStart, Lex::ReturnTokenIndex>;

// A return statement: `return;` or `return expr;`.
struct ReturnStatement {
  static constexpr auto Kind = NodeKind::ReturnStatement.Define(
      {.category = NodeCategory::Statement,
       .bracketed_by = ReturnStatementStart::Kind});

  ReturnStatementStartId introducer;
  std::optional<AnyExprId> expr;
  Lex::SemiTokenIndex token;
};

using IfConditionStart =
    LeafNode<NodeKind::IfConditionStart, Lex::OpenParenTokenIndex>;

// The condition portion of an `if` statement: `(expr)`.
struct IfCondition {
  static constexpr auto Kind = NodeKind::IfCondition.Define(
      {.bracketed_by = IfConditionStart::Kind, .child_count = 2});

  IfConditionStartId left_paren;
  AnyExprId condition;
  Lex::CloseParenTokenIndex token;
};

using IfStatementElse =
    LeafNode<NodeKind::IfStatementElse, Lex::ElseTokenIndex>;

// An `if` statement: `if (expr) { ... } else { ... }`.
struct IfStatement {
  static constexpr auto Kind = NodeKind::IfStatement.Define(
      {.category = NodeCategory::Statement, .bracketed_by = IfCondition::Kind});

  Lex::IfTokenIndex token;
  IfConditionId head;
  CodeBlockId then;

  struct Else {
    IfStatementElseId else_token;
    NodeIdOneOf<CodeBlock, IfStatement> body;
  };
  std::optional<Else> else_clause;
};

using WhileConditionStart =
    LeafNode<NodeKind::WhileConditionStart, Lex::OpenParenTokenIndex>;

// The condition portion of a `while` statement: `(expr)`.
struct WhileCondition {
  static constexpr auto Kind = NodeKind::WhileCondition.Define(
      {.bracketed_by = WhileConditionStart::Kind, .child_count = 2});

  WhileConditionStartId left_paren;
  AnyExprId condition;
  Lex::CloseParenTokenIndex token;
};

// A `while` statement: `while (expr) { ... }`.
struct WhileStatement {
  static constexpr auto Kind =
      NodeKind::WhileStatement.Define({.category = NodeCategory::Statement,
                                       .bracketed_by = WhileCondition::Kind,
                                       .child_count = 2});

  Lex::WhileTokenIndex token;
  WhileConditionId head;
  CodeBlockId body;
};

// Expression nodes
// ----------------

using ParenExprStart =
    LeafNode<NodeKind::ParenExprStart, Lex::OpenParenTokenIndex>;

// A parenthesized expression: `(a)`.
struct ParenExpr {
  static constexpr auto Kind = NodeKind::ParenExpr.Define(
      {.category = NodeCategory::Expr | NodeCategory::MemberExpr,
       .bracketed_by = ParenExprStart::Kind,
       .child_count = 2});

  ParenExprStartId start;
  AnyExprId expr;
  Lex::CloseParenTokenIndex token;
};

// The opening portion of a call expression: `F(`.
struct CallExprStart {
  static constexpr auto Kind =
      NodeKind::CallExprStart.Define({.child_count = 1});

  AnyExprId callee;
  Lex::OpenParenTokenIndex token;
};

// A call expression: `F(a, b, c)`.
struct CallExpr {
  static constexpr auto Kind = NodeKind::CallExpr.Define(
      {.category = NodeCategory::Expr, .bracketed_by = CallExprStart::Kind});

  CallExprStartId start;
  CommaSeparatedList<AnyExprId, PatternListCommaId> arguments;
  Lex::CloseParenTokenIndex token;
};

// A member access expression: `a.b`.
struct MemberAccessExpr {
  static constexpr auto Kind = NodeKind::MemberAccessExpr.Define(
      {.category = NodeCategory::Expr, .child_count = 2});

  AnyExprId lhs;
  Lex::PeriodTokenIndex token;
  AnyMemberAccessId rhs;
};

// Operator and literal templates
// ------------------------------

// A prefix operator expression.
template <const NodeKind& KindT, typename TokenKind>
struct PrefixOperator {
  static constexpr auto Kind =
      KindT.Define({.category = NodeCategory::Expr, .child_count = 1});

  TokenKind token;
  AnyExprId operand;
};

// An infix operator expression.
template <const NodeKind& KindT, typename TokenKind>
struct InfixOperator {
  static constexpr auto Kind =
      KindT.Define({.category = NodeCategory::Expr, .child_count = 2});

  AnyExprId lhs;
  TokenKind token;
  AnyExprId rhs;
};

// A postfix operator expression.
template <const NodeKind& KindT, typename TokenKind>
struct PostfixOperator {
  static constexpr auto Kind =
      KindT.Define({.category = NodeCategory::Expr, .child_count = 1});

  AnyExprId operand;
  TokenKind token;
};

// Literals, operators, and modifiers generated from node_kind.def x-macros.

#define TINYSWIFT_PARSE_NODE_KIND(Name)
#define TINYSWIFT_PARSE_NODE_KIND_TOKEN_LITERAL(Name, LexTokenKind)       \
  using Name = LeafNode<NodeKind::Name, Lex::LexTokenKind##TokenIndex, \
                        NodeCategory::Expr>;
#define TINYSWIFT_PARSE_NODE_KIND_TOKEN_MODIFIER(Name)             \
  using Name##Modifier =                                        \
      LeafNode<NodeKind::Name##Modifier, Lex::Name##TokenIndex, \
               NodeCategory::Modifier>;
#define TINYSWIFT_PARSE_NODE_KIND_PREFIX_OPERATOR(Name) \
  using PrefixOperator##Name =                       \
      PrefixOperator<NodeKind::PrefixOperator##Name, Lex::Name##TokenIndex>;
#define TINYSWIFT_PARSE_NODE_KIND_INFIX_OPERATOR(Name) \
  using InfixOperator##Name =                       \
      InfixOperator<NodeKind::InfixOperator##Name, Lex::Name##TokenIndex>;
#define TINYSWIFT_PARSE_NODE_KIND_POSTFIX_OPERATOR(Name) \
  using PostfixOperator##Name =                       \
      PostfixOperator<NodeKind::PostfixOperator##Name, Lex::Name##TokenIndex>;
#include "toolchain/parse/node_kind.def"

using IntLiteral = LeafNode<NodeKind::IntLiteral, Lex::IntLiteralTokenIndex,
                            NodeCategory::Expr | NodeCategory::IntConst>;

// ---------------------------------------------------------------------------

// A complete source file. Note that there is no corresponding parse node for
// the file. The file is instead the complete contents of the parse tree.
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
