// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "toolchain/parse/parse.h"

#include "common/check.h"
#include "common/vlog.h"
#include "toolchain/parse/context.h"
#include "toolchain/parse/node_kind.h"
#include "toolchain/parse/tree_and_subtrees.h"

namespace TinySwift::Parse {

// Forward declarations from parse_decl.cpp and parse_stmt.cpp.
auto ParseDecl(Context& context) -> void;
auto ParseStatement(Context& context) -> void;

// Determines if the current token starts a declaration.
static auto IsAtDeclStart(Context& context) -> bool {
  auto kind = context.Peek();
  return kind == Lex::TokenKind::LetKeyword ||
         kind == Lex::TokenKind::VarKeyword ||
         kind == Lex::TokenKind::FuncKeyword ||
         kind == Lex::TokenKind::StructKeyword ||
         kind == Lex::TokenKind::ClassKeyword ||
         kind == Lex::TokenKind::EnumKeyword ||
         kind == Lex::TokenKind::ProtocolKeyword ||
         kind == Lex::TokenKind::ExtensionKeyword ||
         kind == Lex::TokenKind::ImportKeyword ||
         kind == Lex::TokenKind::TypealiasKeyword ||
         kind == Lex::TokenKind::InitKeyword ||
         kind == Lex::TokenKind::DeinitKeyword ||
         kind == Lex::TokenKind::SubscriptKeyword ||
         kind == Lex::TokenKind::PublicKeyword ||
         kind == Lex::TokenKind::PrivateKeyword ||
         kind == Lex::TokenKind::InternalKeyword ||
         kind == Lex::TokenKind::FileprivateKeyword ||
         kind == Lex::TokenKind::StaticKeyword ||
         kind == Lex::TokenKind::At;
}

// Parses the top-level content of a source file.
static auto ParseTopLevel(Context& context) -> void {
  // Emit FileStart at position 0 and consume the FileStart token.
  context.AddLeafNode(NodeKind::FileStart, context.Consume());

  // Parse top-level declarations and statements.
  while (!context.AtEndOfFile()) {
    if (IsAtDeclStart(context)) {
      ParseDecl(context);
    } else {
      ParseStatement(context);
    }
  }

  // Emit FileEnd and consume the FileEnd token.
  context.AddLeafNode(NodeKind::FileEnd, context.Consume());
}

auto Parse(Lex::TokenizedBuffer& tokens, ParseOptions options) -> Tree {
  auto* consumer =
      options.consumer ? options.consumer : &Diagnostics::ConsoleConsumer();

  Tree tree(tokens);
  Context context(tree, tokens, *consumer, options.vlog_stream);

  // Run the recursive descent parser.
  ParseTopLevel(context);

  if (options.vlog_stream || options.dump_stream) {
    consumer->Flush();
  }
  TINYSWIFT_VLOG_TO(options.vlog_stream, "*** Parse::Tree ***\n{0}", tree);
  if (options.dump_stream) {
    Parse::TreeAndSubtrees tree_and_subtrees(tokens, tree);
    if (options.dump_preorder_parse_tree) {
      tree_and_subtrees.PrintPreorder(*options.dump_stream);
    } else {
      tree_and_subtrees.Print(*options.dump_stream);
    }
  }
  return tree;
}

}  // namespace TinySwift::Parse
