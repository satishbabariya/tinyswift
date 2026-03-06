// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "toolchain/diagnostics/diagnostic.h"
#include "toolchain/parse/context.h"
#include "toolchain/parse/node_kind.h"
#include "toolchain/parse/typed_nodes.h"

namespace TinySwift::Parse {

namespace {
TINYSWIFT_DIAGNOSTIC(ExpectedDeclNameParser, Error,
                     "expected identifier in declaration");
TINYSWIFT_DIAGNOSTIC(ExpectedDeclParser, Error,
                     "expected declaration");
}  // namespace

// Forward declarations from other parse files.
auto ParseExpr(Context& context) -> void;
auto ParsePattern(Context& context) -> void;
auto ParseType(Context& context) -> void;
auto ParseTypeAnnotation(Context& context) -> void;
auto ParseGenericParameterClause(Context& context) -> void;
auto ParseGenericWhereClause(Context& context) -> void;
auto ParseCodeBlock(Context& context) -> void;
auto ParseStatement(Context& context) -> void;

// Forward declaration for this file (called from other TUs).
auto ParseDecl(Context& context) -> void;
static auto ParseLetDecl(Context& context) -> void;
static auto ParseVarDecl(Context& context) -> void;
static auto ParseFuncDecl(Context& context) -> void;
static auto ParseStructDecl(Context& context) -> void;
static auto ParseClassDecl(Context& context) -> void;
static auto ParseEnumDecl(Context& context) -> void;
static auto ParseProtocolDecl(Context& context) -> void;
static auto ParseExtensionDecl(Context& context) -> void;
static auto ParseImportDecl(Context& context) -> void;
static auto ParseTypealiasDecl(Context& context) -> void;
static auto ParseInitDecl(Context& context) -> void;
static auto ParseDeinitDecl(Context& context) -> void;
static auto ParseSubscriptDecl(Context& context) -> void;
static auto ParseModifiers(Context& context) -> void;
static auto ParseInheritanceClause(Context& context) -> void;
static auto ParseParamList(Context& context) -> void;

// Parses access modifiers: `public`, `private`, `internal`, `fileprivate`.
static auto ParseModifiers(Context& context) -> void {
  while (true) {
    auto kind = context.Peek();
    if (kind == Lex::TokenKind::PublicKeyword ||
        kind == Lex::TokenKind::PrivateKeyword ||
        kind == Lex::TokenKind::InternalKeyword ||
        kind == Lex::TokenKind::FileprivateKeyword) {
      auto token = context.Consume();
      context.AddLeafNode(NodeKind::AccessModifier, token);
      continue;
    }
    if (kind == Lex::TokenKind::StaticKeyword) {
      auto token = context.Consume();
      context.AddLeafNode(NodeKind::StaticModifier, token);
      continue;
    }
    // M112: `comptime` modifier before `func`.
    if (kind == Lex::TokenKind::ComptimeKeyword) {
      // Only treat as modifier if followed by `func`; otherwise it's an expression.
      if (context.PeekNext() == Lex::TokenKind::FuncKeyword) {
        auto token = context.Consume();
        context.AddLeafNode(NodeKind::ComptimeModifier, token);
        continue;
      }
      break;
    }
    // M56: `mutating` is a contextual keyword (plain Identifier token).
    if (kind == Lex::TokenKind::Identifier &&
        context.GetTokenText() == "mutating") {
      auto token = context.Consume();
      context.AddLeafNode(NodeKind::MutatingModifier, token);
      continue;
    }
    break;
  }
}

// Parses @attributes.
static auto ParseAttributes(Context& context) -> void {
  while (context.Peek() == Lex::TokenKind::At) {
    auto at = context.Consume();
    context.AddLeafNode(NodeKind::AttributeStart, at);

    // Parse the attribute name and emit it as a leaf node so the check
    // phase can read the attribute kind (e.g. "extern", "cdecl").
    if (context.Peek() == Lex::TokenKind::Identifier) {
      auto name_tok = context.Consume();
      context.AddLeafNode(NodeKind::IdentifierNameNotBeforeParams, name_tok);
    }

    // Parse optional parenthesized arguments and emit each token as a leaf
    // node so the check phase can extract attribute arguments (e.g. "C",
    // "get_answer").
    if (context.Peek() == Lex::TokenKind::OpenParen) {
      auto open = context.Consume();
      auto close = context.GetMatchedClosingToken(open);
      // Emit interior tokens as leaf nodes.
      while (context.Peek() != Lex::TokenKind::CloseParen &&
             context.position() < close) {
        auto tok = context.Consume();
        auto tok_kind = context.tokens().GetKind(tok);
        if (tok_kind == Lex::TokenKind::StringLiteral) {
          context.AddLeafNode(NodeKind::StringLiteral, tok);
        } else if (tok_kind == Lex::TokenKind::Identifier) {
          context.AddLeafNode(NodeKind::IdentifierNameNotBeforeParams, tok);
        }
        // Skip commas and other punctuation.
      }
      context.Consume();  // ')'
    }

    context.AddNode(NodeKind::Attribute, at);
  }
}

// Parses an inheritance clause: `: TypeA, TypeB`
static auto ParseInheritanceClause(Context& context) -> void {
  auto colon = context.Consume();  // ':'
  context.AddLeafNode(NodeKind::InheritanceClauseStart, colon);

  // Parse first inherited type.
  if (context.Peek() == Lex::TokenKind::Identifier ||
      context.Peek() == Lex::TokenKind::AnyKeyword ||
      context.Peek() == Lex::TokenKind::CapitalSelfKeyword) {
    auto name = context.Consume();
    context.AddLeafNode(NodeKind::IdentifierType, name);
  }

  // Parse additional inherited types.
  while (context.Peek() == Lex::TokenKind::Comma) {
    auto comma = context.Consume();
    context.AddLeafNode(NodeKind::PatternListComma, comma);
    if (context.Peek() == Lex::TokenKind::Identifier ||
        context.Peek() == Lex::TokenKind::AnyKeyword ||
        context.Peek() == Lex::TokenKind::CapitalSelfKeyword) {
      auto name = context.Consume();
      context.AddLeafNode(NodeKind::IdentifierType, name);
    }
  }

  context.AddNode(NodeKind::InheritanceClause, colon);
}

// Parses a parameter list: `(name: Type, name: Type, ...)`
static auto ParseParamList(Context& context) -> void {
  auto open = context.ConsumeChecked(Lex::TokenKind::OpenParen);
  context.AddLeafNode(NodeKind::ExplicitParamListStart, open);

  if (context.Peek() != Lex::TokenKind::CloseParen) {
    // Parse first parameter.
    auto ParseOneParam = [&]() {
      auto param_start = context.position();

      // Check for external parameter name: `external internal: Type`
      // or just `name: Type`
      if (context.Peek() == Lex::TokenKind::Identifier ||
          context.Peek() == Lex::TokenKind::UnderscoreKeyword) {
        auto first = context.Consume();

        if (context.Peek() == Lex::TokenKind::Identifier) {
          // Two names: `external internal: Type`
          context.AddLeafNode(NodeKind::ExternalParamName, first);
          auto internal_name = context.Consume();
          context.AddLeafNode(NodeKind::IdentifierPattern, internal_name);
        } else if (context.Peek() == Lex::TokenKind::Colon) {
          // One name: `name: Type`
          context.AddLeafNode(NodeKind::IdentifierPattern, first);
        } else {
          // Just an identifier, might be a pattern.
          context.AddLeafNode(NodeKind::IdentifierPattern, first);
        }
      }

      // Parse type annotation.
      if (context.Peek() == Lex::TokenKind::Colon) {
        ParseTypeAnnotation(context);
      }

      // Optional default value.
      if (context.Peek() == Lex::TokenKind::Equal) {
        context.Consume();
        ParseExpr(context);
      }

      context.AddNode(NodeKind::FunctionParam, param_start);
    };

    ParseOneParam();
    while (context.Peek() == Lex::TokenKind::Comma) {
      auto comma = context.Consume();
      context.AddLeafNode(NodeKind::PatternListComma, comma);
      ParseOneParam();
    }
  }

  auto close = context.ConsumeChecked(Lex::TokenKind::CloseParen);
  context.AddNode(NodeKind::ExplicitParamList, close);
}

// Parses a `let` declaration: `let name: Type = expr`
static auto ParseLetDecl(Context& context) -> void {
  auto let_token = context.Consume();  // 'let'
  context.AddLeafNode(NodeKind::LetIntroducer, let_token);

  // Parse the binding pattern.
  ParsePattern(context);

  // Optional initializer.
  if (context.Peek() == Lex::TokenKind::Equal) {
    auto eq = context.Consume();
    ParseExpr(context);
    context.AddNode(NodeKind::LetInitializer, eq);
  }

  context.AddNode(NodeKind::LetDecl, let_token);
}

// Parses a `var` declaration: `var name: Type = expr`
static auto ParseVarDecl(Context& context) -> void {
  auto var_token = context.Consume();  // 'var'
  context.AddLeafNode(NodeKind::VariableIntroducer, var_token);

  // Parse the binding pattern.
  ParsePattern(context);

  // Optional initializer.
  if (context.Peek() == Lex::TokenKind::Equal) {
    auto eq = context.Consume();
    ParseExpr(context);
    context.AddNode(NodeKind::VariableInitializer, eq);
  }

  // Optional accessor block: `{ get { } set { } }`
  if (context.Peek() == Lex::TokenKind::OpenCurlyBrace) {
    auto open = context.Consume();
    context.AddLeafNode(NodeKind::AccessorBlockStart, open);

    while (context.Peek() != Lex::TokenKind::CloseCurlyBrace &&
           !context.AtEndOfFile()) {
      auto accessor_text = context.GetTokenText();

      if (accessor_text == "get") {
        auto get_token = context.Consume();
        if (context.Peek() == Lex::TokenKind::OpenCurlyBrace) {
          ParseCodeBlock(context);
        }
        context.AddNode(NodeKind::GetAccessor, get_token);
      } else if (accessor_text == "set") {
        auto set_token = context.Consume();
        // Optional parameter name.
        if (context.Peek() == Lex::TokenKind::OpenParen) {
          context.Consume();
          if (context.Peek() == Lex::TokenKind::Identifier) {
            context.Consume();
          }
          context.ConsumeChecked(Lex::TokenKind::CloseParen);
        }
        if (context.Peek() == Lex::TokenKind::OpenCurlyBrace) {
          ParseCodeBlock(context);
        }
        context.AddNode(NodeKind::SetAccessor, set_token);
      } else if (accessor_text == "willSet") {
        auto ws_token = context.Consume();
        if (context.Peek() == Lex::TokenKind::OpenParen) {
          context.Consume();
          if (context.Peek() == Lex::TokenKind::Identifier) {
            context.Consume();
          }
          context.ConsumeChecked(Lex::TokenKind::CloseParen);
        }
        if (context.Peek() == Lex::TokenKind::OpenCurlyBrace) {
          ParseCodeBlock(context);
        }
        context.AddNode(NodeKind::WillSetAccessor, ws_token);
      } else if (accessor_text == "didSet") {
        auto ds_token = context.Consume();
        if (context.Peek() == Lex::TokenKind::OpenParen) {
          context.Consume();
          if (context.Peek() == Lex::TokenKind::Identifier) {
            context.Consume();
          }
          context.ConsumeChecked(Lex::TokenKind::CloseParen);
        }
        if (context.Peek() == Lex::TokenKind::OpenCurlyBrace) {
          ParseCodeBlock(context);
        }
        context.AddNode(NodeKind::DidSetAccessor, ds_token);
      } else {
        // This is a computed property with a direct body (no explicit `get`).
        // Parse it as an implicit getter.
        ParseStatement(context);
      }
    }

    auto close = context.ConsumeChecked(Lex::TokenKind::CloseCurlyBrace);
    context.AddNode(NodeKind::AccessorBlock, close);
  }

  context.AddNode(NodeKind::VariableDecl, var_token);
}

// Parses a `func` declaration.
static auto ParseFuncDecl(Context& context) -> void {
  auto func_token = context.Consume();  // 'func'
  context.AddLeafNode(NodeKind::FunctionIntroducer, func_token);

  // Parse the function name.
  if (context.Peek() == Lex::TokenKind::Identifier) {
    auto name = context.Consume();
    context.AddLeafNode(NodeKind::IdentifierNameBeforeParams, name);
  } else if (context.Peek() == Lex::TokenKind::OperatorBinarySpaced ||
             context.Peek() == Lex::TokenKind::OperatorBinaryUnspaced ||
             context.Peek() == Lex::TokenKind::OperatorPrefix ||
             context.Peek() == Lex::TokenKind::OperatorPostfix) {
    // Operator function: `func +(lhs: T, rhs: T) -> T`
    auto name = context.Consume();
    context.AddLeafNode(NodeKind::IdentifierNameBeforeParams, name);
  } else {
    context.EmitError(ExpectedDeclNameParser);
  }

  // Optional generic parameter clause.
  if (context.Peek() == Lex::TokenKind::OperatorBinaryUnspaced ||
      context.Peek() == Lex::TokenKind::OperatorPrefix) {
    auto text = context.GetTokenText();
    if (text == "<") {
      ParseGenericParameterClause(context);
    }
  }

  // Parse parameter list.
  if (context.Peek() == Lex::TokenKind::OpenParen) {
    ParseParamList(context);
  }

  // Optional `throws` / `rethrows`.
  if (context.Peek() == Lex::TokenKind::ThrowsKeyword ||
      context.Peek() == Lex::TokenKind::RethrowsKeyword) {
    context.Consume();
  }

  // M100: Optional `async` modifier before return type.
  if (context.Peek() == Lex::TokenKind::AsyncKeyword) {
    auto async_token = context.Consume();
    context.AddLeafNode(NodeKind::AsyncModifier, async_token);
  }

  // Optional return type: `-> Type`
  if (context.Peek() == Lex::TokenKind::MinusGreater) {
    auto arrow = context.Consume();
    ParseType(context);
    context.AddNode(NodeKind::ReturnType, arrow);
  }

  // Optional where clause.
  if (context.Peek() == Lex::TokenKind::WhereKeyword) {
    ParseGenericWhereClause(context);
  }

  // Function body or forward declaration.
  if (context.Peek() == Lex::TokenKind::OpenCurlyBrace) {
    // Function definition: FunctionIntroducer ... FunctionDefinitionStart
    //                      CodeBlock FunctionDefinition
    context.AddNode(NodeKind::FunctionDefinitionStart, func_token);
    ParseCodeBlock(context);
    context.AddNode(NodeKind::FunctionDefinition, func_token);
  } else {
    // Forward declaration: FunctionIntroducer ... FunctionDecl
    context.AddNode(NodeKind::FunctionDecl, func_token);
  }
}

// Parses members inside a type body: `{ members... }`
static auto ParseMemberBlock(Context& context) -> void {
  auto open = context.ConsumeChecked(Lex::TokenKind::OpenCurlyBrace);
  context.AddLeafNode(NodeKind::CodeBlockStart, open);

  while (context.Peek() != Lex::TokenKind::CloseCurlyBrace &&
         !context.AtEndOfFile()) {
    // Skip semicolons as member separators.
    while (context.Peek() == Lex::TokenKind::Semi) {
      context.Consume();
    }
    if (context.Peek() == Lex::TokenKind::CloseCurlyBrace) break;
    ParseDecl(context);
  }

  auto close = context.ConsumeChecked(Lex::TokenKind::CloseCurlyBrace);
  context.AddNode(NodeKind::CodeBlock, close);
}

// Parses a `struct` declaration.
static auto ParseStructDecl(Context& context) -> void {
  auto struct_token = context.Consume();  // 'struct'
  context.AddLeafNode(NodeKind::StructIntroducer, struct_token);

  // Name.
  if (context.Peek() == Lex::TokenKind::Identifier) {
    auto name = context.Consume();
    context.AddLeafNode(NodeKind::IdentifierNameNotBeforeParams, name);
  } else {
    context.EmitError(ExpectedDeclNameParser);
  }

  // Optional generic parameter clause.
  if (context.Peek() == Lex::TokenKind::OperatorBinaryUnspaced ||
      context.Peek() == Lex::TokenKind::OperatorPrefix) {
    auto text = context.GetTokenText();
    if (text == "<") {
      ParseGenericParameterClause(context);
    }
  }

  // Optional inheritance clause.
  if (context.Peek() == Lex::TokenKind::Colon) {
    ParseInheritanceClause(context);
  }

  // Optional where clause.
  if (context.Peek() == Lex::TokenKind::WhereKeyword) {
    ParseGenericWhereClause(context);
  }

  context.AddNode(NodeKind::StructDefinitionStart, struct_token);

  // Body.
  if (context.Peek() == Lex::TokenKind::OpenCurlyBrace) {
    ParseMemberBlock(context);
    context.AddNode(NodeKind::StructDefinition, struct_token);
  }
}

// Parses a `class` declaration.
static auto ParseClassDecl(Context& context) -> void {
  auto class_token = context.Consume();  // 'class'
  context.AddLeafNode(NodeKind::ClassIntroducer, class_token);

  // Name.
  if (context.Peek() == Lex::TokenKind::Identifier) {
    auto name = context.Consume();
    context.AddLeafNode(NodeKind::IdentifierNameNotBeforeParams, name);
  } else {
    context.EmitError(ExpectedDeclNameParser);
  }

  // Optional generic parameter clause.
  if (context.Peek() == Lex::TokenKind::OperatorBinaryUnspaced ||
      context.Peek() == Lex::TokenKind::OperatorPrefix) {
    auto text = context.GetTokenText();
    if (text == "<") {
      ParseGenericParameterClause(context);
    }
  }

  // Optional inheritance clause.
  if (context.Peek() == Lex::TokenKind::Colon) {
    ParseInheritanceClause(context);
  }

  // Optional where clause.
  if (context.Peek() == Lex::TokenKind::WhereKeyword) {
    ParseGenericWhereClause(context);
  }

  context.AddNode(NodeKind::ClassDefinitionStart, class_token);

  // Body.
  if (context.Peek() == Lex::TokenKind::OpenCurlyBrace) {
    ParseMemberBlock(context);
    context.AddNode(NodeKind::ClassDefinition, class_token);
  }
}

// Parses an `enum` declaration.
static auto ParseEnumDecl(Context& context) -> void {
  auto enum_token = context.Consume();  // 'enum'
  context.AddLeafNode(NodeKind::EnumIntroducer, enum_token);

  // Name.
  if (context.Peek() == Lex::TokenKind::Identifier) {
    auto name = context.Consume();
    context.AddLeafNode(NodeKind::IdentifierNameNotBeforeParams, name);
  } else {
    context.EmitError(ExpectedDeclNameParser);
  }

  // Optional generic parameter clause.
  if (context.Peek() == Lex::TokenKind::OperatorBinaryUnspaced ||
      context.Peek() == Lex::TokenKind::OperatorPrefix) {
    auto text = context.GetTokenText();
    if (text == "<") {
      ParseGenericParameterClause(context);
    }
  }

  // Optional inheritance clause.
  if (context.Peek() == Lex::TokenKind::Colon) {
    ParseInheritanceClause(context);
  }

  context.AddNode(NodeKind::EnumDefinitionStart, enum_token);

  // Body.
  if (context.Peek() == Lex::TokenKind::OpenCurlyBrace) {
    auto open = context.Consume();
    context.AddLeafNode(NodeKind::CodeBlockStart, open);

    while (context.Peek() != Lex::TokenKind::CloseCurlyBrace &&
           !context.AtEndOfFile()) {
      if (context.Peek() == Lex::TokenKind::CaseKeyword) {
        auto case_token = context.Consume();
        context.AddLeafNode(NodeKind::EnumCaseIntroducer, case_token);

        // Parse case elements (comma-separated).
        auto ParseCaseElement = [&]() {
          auto elem_start = context.position();

          // Consume the case name.
          std::optional<Lex::TokenIndex> name_tok;
          if (context.Peek() == Lex::TokenKind::Identifier) {
            name_tok = context.Consume();
          }

          // Consume optional associated value `(Type)`.
          // We emit IdentifierType BEFORE IdentifierNameNotBeforeParams so
          // that EnumCaseElement.child_count=1 picks up the name as its child,
          // and IdentifierType falls through to become an EnumCaseDecl child.
          // This avoids FunctionParam.child_count=2 bracket-validation issues.
          std::optional<Lex::TokenIndex> type_tok;
          if (context.Peek() == Lex::TokenKind::OpenParen) {
            context.Consume();  // '('
            if (context.Peek() == Lex::TokenKind::Identifier ||
                context.Peek() == Lex::TokenKind::AnyKeyword ||
                context.Peek() == Lex::TokenKind::CapitalSelfKeyword) {
              type_tok = context.Consume();
            }
            if (context.Peek() == Lex::TokenKind::CloseParen) {
              context.Consume();  // ')'
            }
          }

          // Optional raw value: `= value`
          if (context.Peek() == Lex::TokenKind::Equal) {
            context.Consume();
            ParseExpr(context);
          }

          // Emit in reverse order: type first (→ EnumCaseDecl child),
          // name second (→ EnumCaseElement child via child_count=1).
          if (type_tok.has_value()) {
            context.AddLeafNode(NodeKind::IdentifierType, *type_tok);
          }
          if (name_tok.has_value()) {
            context.AddLeafNode(NodeKind::IdentifierNameNotBeforeParams, *name_tok);
          }

          context.AddNode(NodeKind::EnumCaseElement, elem_start);
        };

        ParseCaseElement();
        while (context.Peek() == Lex::TokenKind::Comma) {
          auto comma = context.Consume();
          context.AddLeafNode(NodeKind::PatternListComma, comma);
          ParseCaseElement();
        }

        context.AddNode(NodeKind::EnumCaseDecl, case_token);
      } else {
        // Other declarations within the enum.
        ParseDecl(context);
      }
    }

    auto close = context.ConsumeChecked(Lex::TokenKind::CloseCurlyBrace);
    context.AddNode(NodeKind::CodeBlock, close);
    context.AddNode(NodeKind::EnumDefinition, enum_token);
  }
}

// Parses a `protocol` declaration.
static auto ParseProtocolDecl(Context& context) -> void {
  auto proto_token = context.Consume();  // 'protocol'
  context.AddLeafNode(NodeKind::ProtocolIntroducer, proto_token);

  // Name.
  if (context.Peek() == Lex::TokenKind::Identifier) {
    auto name = context.Consume();
    context.AddLeafNode(NodeKind::IdentifierNameNotBeforeParams, name);
  } else {
    context.EmitError(ExpectedDeclNameParser);
  }

  // Optional inheritance clause.
  if (context.Peek() == Lex::TokenKind::Colon) {
    ParseInheritanceClause(context);
  }

  // Optional where clause.
  if (context.Peek() == Lex::TokenKind::WhereKeyword) {
    ParseGenericWhereClause(context);
  }

  context.AddNode(NodeKind::ProtocolDefinitionStart, proto_token);

  // Body.
  if (context.Peek() == Lex::TokenKind::OpenCurlyBrace) {
    ParseMemberBlock(context);
    context.AddNode(NodeKind::ProtocolDefinition, proto_token);
  }
}

// Parses an `extension` declaration.
static auto ParseExtensionDecl(Context& context) -> void {
  auto ext_token = context.Consume();  // 'extension'
  context.AddLeafNode(NodeKind::ExtensionIntroducer, ext_token);

  // Extended type name.
  if (context.Peek() == Lex::TokenKind::Identifier) {
    auto name = context.Consume();
    context.AddLeafNode(NodeKind::IdentifierType, name);
  }

  // Optional inheritance clause.
  if (context.Peek() == Lex::TokenKind::Colon) {
    ParseInheritanceClause(context);
  }

  // Optional where clause.
  if (context.Peek() == Lex::TokenKind::WhereKeyword) {
    ParseGenericWhereClause(context);
  }

  context.AddNode(NodeKind::ExtensionDefinitionStart, ext_token);

  // Body.
  if (context.Peek() == Lex::TokenKind::OpenCurlyBrace) {
    ParseMemberBlock(context);
    context.AddNode(NodeKind::ExtensionDefinition, ext_token);
  }
}

// Parses an `import` declaration: `import Module` or `import Module.Sub`
static auto ParseImportDecl(Context& context) -> void {
  auto import_token = context.Consume();  // 'import'
  context.AddLeafNode(NodeKind::ImportIntroducer, import_token);

  // Parse module path: `Module.Submodule.Symbol`
  if (context.Peek() == Lex::TokenKind::Identifier) {
    auto name = context.Consume();
    context.AddLeafNode(NodeKind::IdentifierNameNotBeforeParams, name);

    while (context.Peek() == Lex::TokenKind::Period) {
      context.Consume();
      if (context.Peek() == Lex::TokenKind::Identifier) {
        auto sub = context.Consume();
        context.AddLeafNode(NodeKind::IdentifierNameNotBeforeParams, sub);
      }
    }
  }

  context.AddNode(NodeKind::ImportDecl, import_token);
}

// Parses a `typealias` declaration: `typealias Name = Type`
static auto ParseTypealiasDecl(Context& context) -> void {
  auto alias_token = context.Consume();  // 'typealias'
  context.AddLeafNode(NodeKind::TypealiasIntroducer, alias_token);

  // Name.
  if (context.Peek() == Lex::TokenKind::Identifier) {
    auto name = context.Consume();
    context.AddLeafNode(NodeKind::IdentifierNameNotBeforeParams, name);
  }

  // Optional generic parameter clause.
  if (context.Peek() == Lex::TokenKind::OperatorBinaryUnspaced ||
      context.Peek() == Lex::TokenKind::OperatorPrefix) {
    auto text = context.GetTokenText();
    if (text == "<") {
      ParseGenericParameterClause(context);
    }
  }

  // `= Type`
  if (context.Peek() == Lex::TokenKind::Equal) {
    context.Consume();
    // Parse the aliased type - forward to type parsing.
    // We need to parse a type expression here. For simplicity, we'll use
    // the type annotation parser minus the colon.
    if (context.Peek() == Lex::TokenKind::Identifier ||
        context.Peek() == Lex::TokenKind::AnyKeyword ||
        context.Peek() == Lex::TokenKind::CapitalSelfKeyword) {
      auto type_name = context.Consume();
      context.AddLeafNode(NodeKind::IdentifierType, type_name);
    } else if (context.Peek() == Lex::TokenKind::OpenSquareBracket ||
               context.Peek() == Lex::TokenKind::OpenParen) {
      // Complex type - delegate to expression parsing.
      ParseExpr(context);
    }
  }

  context.AddNode(NodeKind::TypealiasDecl, alias_token);
}

// Parses an `init` declaration.
static auto ParseInitDecl(Context& context) -> void {
  auto init_token = context.Consume();  // 'init'
  context.AddLeafNode(NodeKind::InitIntroducer, init_token);

  // Optional `?` or `!` for failable initializers.
  if (context.Peek() == Lex::TokenKind::Question ||
      context.Peek() == Lex::TokenKind::Exclaim) {
    context.Consume();
  }

  // Optional generic parameter clause.
  if (context.Peek() == Lex::TokenKind::OperatorBinaryUnspaced ||
      context.Peek() == Lex::TokenKind::OperatorPrefix) {
    auto text = context.GetTokenText();
    if (text == "<") {
      ParseGenericParameterClause(context);
    }
  }

  // Parameter list.
  if (context.Peek() == Lex::TokenKind::OpenParen) {
    ParseParamList(context);
  }

  // Optional `throws`.
  if (context.Peek() == Lex::TokenKind::ThrowsKeyword) {
    context.Consume();
  }

  // Optional where clause.
  if (context.Peek() == Lex::TokenKind::WhereKeyword) {
    ParseGenericWhereClause(context);
  }

  context.AddNode(NodeKind::InitDefinitionStart, init_token);

  // Body.
  if (context.Peek() == Lex::TokenKind::OpenCurlyBrace) {
    ParseCodeBlock(context);
    context.AddNode(NodeKind::InitDefinition, init_token);
  }
}

// Parses a `deinit` declaration.
static auto ParseDeinitDecl(Context& context) -> void {
  auto deinit_token = context.Consume();  // 'deinit'
  context.AddLeafNode(NodeKind::DeinitIntroducer, deinit_token);

  // Body.
  if (context.Peek() == Lex::TokenKind::OpenCurlyBrace) {
    ParseCodeBlock(context);
  }

  context.AddNode(NodeKind::DeinitDefinition, deinit_token);
}

// Parses a `subscript` declaration.
static auto ParseSubscriptDecl(Context& context) -> void {
  auto sub_token = context.Consume();  // 'subscript'
  context.AddLeafNode(NodeKind::SubscriptIntroducer, sub_token);

  // Optional generic parameter clause.
  if (context.Peek() == Lex::TokenKind::OperatorBinaryUnspaced ||
      context.Peek() == Lex::TokenKind::OperatorPrefix) {
    auto text = context.GetTokenText();
    if (text == "<") {
      ParseGenericParameterClause(context);
    }
  }

  // Parameter list (using square brackets actually, but modeled as parens).
  if (context.Peek() == Lex::TokenKind::OpenParen) {
    ParseParamList(context);
  }

  // Return type.
  if (context.Peek() == Lex::TokenKind::MinusGreater) {
    auto arrow = context.Consume();
    ParseType(context);
    context.AddNode(NodeKind::ReturnType, arrow);
  }

  context.AddNode(NodeKind::SubscriptDefinitionStart, sub_token);

  // Accessor block.
  if (context.Peek() == Lex::TokenKind::OpenCurlyBrace) {
    auto open = context.Consume();
    context.AddLeafNode(NodeKind::AccessorBlockStart, open);

    while (context.Peek() != Lex::TokenKind::CloseCurlyBrace &&
           !context.AtEndOfFile()) {
      auto accessor_text = context.GetTokenText();

      if (accessor_text == "get") {
        auto get_token = context.Consume();
        if (context.Peek() == Lex::TokenKind::OpenCurlyBrace) {
          ParseCodeBlock(context);
        }
        context.AddNode(NodeKind::GetAccessor, get_token);
      } else if (accessor_text == "set") {
        auto set_token = context.Consume();
        if (context.Peek() == Lex::TokenKind::OpenParen) {
          context.Consume();
          if (context.Peek() == Lex::TokenKind::Identifier) {
            context.Consume();
          }
          context.ConsumeChecked(Lex::TokenKind::CloseParen);
        }
        if (context.Peek() == Lex::TokenKind::OpenCurlyBrace) {
          ParseCodeBlock(context);
        }
        context.AddNode(NodeKind::SetAccessor, set_token);
      } else {
        // Unexpected - skip.
        context.Consume();
      }
    }

    auto close = context.ConsumeChecked(Lex::TokenKind::CloseCurlyBrace);
    context.AddNode(NodeKind::AccessorBlock, close);
    context.AddNode(NodeKind::SubscriptDefinition, sub_token);
  }
}

// Main declaration dispatch.
auto ParseDecl(Context& context) -> void {
  // Parse attributes first.
  if (context.Peek() == Lex::TokenKind::At) {
    ParseAttributes(context);
  }

  // Parse modifiers (public, private, static, etc.).
  ParseModifiers(context);

  auto kind = context.Peek();

  if (kind == Lex::TokenKind::LetKeyword) {
    ParseLetDecl(context);
    return;
  }
  if (kind == Lex::TokenKind::VarKeyword) {
    ParseVarDecl(context);
    return;
  }
  if (kind == Lex::TokenKind::FuncKeyword) {
    ParseFuncDecl(context);
    return;
  }
  if (kind == Lex::TokenKind::StructKeyword) {
    ParseStructDecl(context);
    return;
  }
  if (kind == Lex::TokenKind::ClassKeyword) {
    ParseClassDecl(context);
    return;
  }
  if (kind == Lex::TokenKind::EnumKeyword) {
    ParseEnumDecl(context);
    return;
  }
  if (kind == Lex::TokenKind::ProtocolKeyword) {
    ParseProtocolDecl(context);
    return;
  }
  if (kind == Lex::TokenKind::ExtensionKeyword) {
    ParseExtensionDecl(context);
    return;
  }
  if (kind == Lex::TokenKind::ImportKeyword) {
    ParseImportDecl(context);
    return;
  }
  if (kind == Lex::TokenKind::TypealiasKeyword) {
    ParseTypealiasDecl(context);
    return;
  }
  if (kind == Lex::TokenKind::InitKeyword) {
    ParseInitDecl(context);
    return;
  }
  if (kind == Lex::TokenKind::DeinitKeyword) {
    ParseDeinitDecl(context);
    return;
  }
  if (kind == Lex::TokenKind::SubscriptKeyword) {
    ParseSubscriptDecl(context);
    return;
  }

  // Unrecognized declaration.
  context.EmitError(ExpectedDeclParser);
  context.SkipToNextDecl();
}

}  // namespace TinySwift::Parse
