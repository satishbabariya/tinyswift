// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "toolchain/parse/context.h"

#include "common/check.h"
#include "toolchain/diagnostics/diagnostic.h"

namespace TinySwift::Parse {

Context::Context(Tree& tree, Lex::TokenizedBuffer& tokens,
                 Diagnostics::Consumer& consumer,
                 llvm::raw_ostream* vlog)
    : tree_(tree),
      tokens_(&tokens),
      emitter_(&consumer, &tokens),
      current_(Lex::TokenIndex(0)),
      vlog_(vlog) {}

auto Context::PeekNext() const -> Lex::TokenKind {
  Lex::TokenIndex next(current_.index + 1);
  return tokens_->GetKind(next);
}

auto Context::AtStartOfLine() const -> bool {
  if (current_.index == 0) {
    return true;
  }
  Lex::TokenIndex prev(current_.index - 1);
  return tokens_->GetLine(current_) != tokens_->GetLine(prev);
}

auto Context::Consume() -> Lex::TokenIndex {
  auto token = current_;
  current_ = Lex::TokenIndex(current_.index + 1);
  return token;
}

auto Context::ConsumeIf(Lex::TokenKind kind) -> std::optional<Lex::TokenIndex> {
  if (Peek() == kind) {
    return Consume();
  }
  return std::nullopt;
}

auto Context::ConsumeChecked(Lex::TokenKind kind) -> Lex::TokenIndex {
  TINYSWIFT_CHECK(Peek() == kind, "Expected {0}, got {1}", kind, Peek());
  return Consume();
}

auto Context::AddLeafNode(NodeKind kind, Lex::TokenIndex token,
                          bool has_error) -> NodeId {
  auto id = NodeId(tree_.node_impls_.size());
  tree_.node_impls_.push_back(Tree::NodeImpl(kind, has_error, token));
  return id;
}

auto Context::AddNode(NodeKind kind, Lex::TokenIndex token,
                      bool has_error) -> NodeId {
  return AddLeafNode(kind, token, has_error);
}

auto Context::SkipToNextDecl() -> void {
  while (!AtEndOfFile()) {
    auto kind = Peek();
    // If we're at the start of a line and see a declaration introducer, stop.
    if (AtStartOfLine() && kind.is_keyword()) {
      if (kind == Lex::TokenKind::FuncKeyword ||
          kind == Lex::TokenKind::LetKeyword ||
          kind == Lex::TokenKind::VarKeyword ||
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
          kind == Lex::TokenKind::StaticKeyword) {
        return;
      }
    }
    // Also stop at closing braces on a new line.
    if (AtStartOfLine() && kind == Lex::TokenKind::CloseCurlyBrace) {
      return;
    }
    Consume();
  }
}

auto Context::SkipTo(Lex::TokenIndex target) -> void {
  while (current_ < target && !AtEndOfFile()) {
    Consume();
  }
}

auto Context::EmitInvalidParse(Lex::TokenIndex start) -> void {
  // Emit InvalidParseStart at the start token, then skip tokens up to
  // current, emitting InvalidParse for each, then close with
  // InvalidParseSubtree.
  // Simplified: just add InvalidParse nodes for all skipped tokens.
  auto start_token = start;
  AddLeafNode(NodeKind::InvalidParseStart, start_token, true);
  // The InvalidParseSubtree will encompass from start to current.
  while (position() != start && position().index > start_token.index) {
    // Already consumed.
    break;
  }
  AddNode(NodeKind::InvalidParseSubtree, position(), true);
  tree_.set_has_errors(true);
}

}  // namespace TinySwift::Parse
