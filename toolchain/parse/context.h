// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef TINYSWIFT_TOOLCHAIN_PARSE_CONTEXT_H_
#define TINYSWIFT_TOOLCHAIN_PARSE_CONTEXT_H_

#include <optional>

#include "common/ostream.h"
#include "toolchain/diagnostics/diagnostic.h"
#include "toolchain/diagnostics/emitter.h"
#include "toolchain/lex/token_index.h"
#include "toolchain/lex/token_kind.h"
#include "toolchain/lex/tokenized_buffer.h"
#include "toolchain/parse/node_ids.h"
#include "toolchain/parse/node_kind.h"
#include "toolchain/parse/tree.h"

namespace TinySwift::Parse {

// The parser context. This is a friend of Tree and pushes NodeImpl entries into
// the tree in depth-first postorder. The recursive descent parser functions
// operate on this context.
class Context {
 public:
  explicit Context(Tree& tree, Lex::TokenizedBuffer& tokens,
                   Diagnostics::Consumer& consumer,
                   llvm::raw_ostream* vlog);

  // -------------------------------------------------------------------------
  // Token access
  // -------------------------------------------------------------------------

  // Returns the current token position.
  auto position() const -> Lex::TokenIndex { return current_; }

  // Returns the kind of the current token.
  auto Peek() const -> Lex::TokenKind {
    return tokens_->GetKind(current_);
  }

  // Returns the kind of the token after the current one.
  auto PeekNext() const -> Lex::TokenKind;

  // Returns the text of the current token.
  auto GetTokenText() const -> llvm::StringRef {
    return tokens_->GetTokenText(current_);
  }

  // Returns the text of a specific token.
  auto GetTokenText(Lex::TokenIndex token) const -> llvm::StringRef {
    return tokens_->GetTokenText(token);
  }

  // Returns the token kind of a specific token.
  auto GetKind(Lex::TokenIndex token) const -> Lex::TokenKind {
    return tokens_->GetKind(token);
  }

  // Returns true if the current token starts a new line compared to the
  // previous token.
  auto AtStartOfLine() const -> bool;

  // Returns true if we're at the end of the file.
  auto AtEndOfFile() const -> bool {
    return Peek() == Lex::TokenKind::FileEnd;
  }

  // Consumes the current token and advances. Returns the consumed token.
  auto Consume() -> Lex::TokenIndex;

  // Consumes the current token if it matches the specified kind.
  auto ConsumeIf(Lex::TokenKind kind) -> std::optional<Lex::TokenIndex>;

  // Consumes the current token, checking that it matches the expected kind.
  // Emits an error diagnostic if it doesn't match.
  auto ConsumeChecked(Lex::TokenKind kind) -> Lex::TokenIndex;

  // -------------------------------------------------------------------------
  // Tree building (pushes NodeImpl in postorder)
  // -------------------------------------------------------------------------

  // Adds a leaf node (zero children) for the given token.
  auto AddLeafNode(NodeKind kind, Lex::TokenIndex token,
                   bool has_error = false) -> NodeId;

  // Adds a node for the given token. Used for non-leaf nodes after their
  // children have already been pushed.
  auto AddNode(NodeKind kind, Lex::TokenIndex token,
               bool has_error = false) -> NodeId;

  // -------------------------------------------------------------------------
  // Error recovery
  // -------------------------------------------------------------------------

  // Skips tokens until we find a likely declaration start at the beginning
  // of a line, or reach the end of file.
  auto SkipToNextDecl() -> void;

  // Skips to a specific token position.
  auto SkipTo(Lex::TokenIndex target) -> void;

  // Gets the matched closing token for an opening grouping token.
  auto GetMatchedClosingToken(Lex::TokenIndex open) -> Lex::TokenIndex {
    return tokens_->GetMatchedClosingToken(open);
  }

  // Emits an InvalidParseSubtree covering tokens from start to current.
  auto EmitInvalidParse(Lex::TokenIndex start) -> void;

  // -------------------------------------------------------------------------
  // Diagnostics
  // -------------------------------------------------------------------------

  // Emits a diagnostic at the current token position.
  template <typename... Args>
  auto EmitError(const Diagnostics::DiagnosticBase<Args...>& diagnostic,
                 Diagnostics::Internal::NoTypeDeduction<Args>... args)
      -> void {
    emitter_.Emit(current_, diagnostic, args...);
    tree_.set_has_errors(true);
  }

  // Emits a diagnostic at a specified token position.
  template <typename... Args>
  auto EmitErrorAt(Lex::TokenIndex token,
                   const Diagnostics::DiagnosticBase<Args...>& diagnostic,
                   Diagnostics::Internal::NoTypeDeduction<Args>... args)
      -> void {
    emitter_.Emit(token, diagnostic, args...);
    tree_.set_has_errors(true);
  }

  // -------------------------------------------------------------------------
  // Tree/token access
  // -------------------------------------------------------------------------

  auto tree() -> Tree& { return tree_; }
  auto tokens() -> Lex::TokenizedBuffer& { return *tokens_; }

 private:
  // Diagnostic emitter for parser errors, locating by token index.
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

  Tree& tree_;
  Lex::TokenizedBuffer* tokens_;
  TokenEmitter emitter_;
  Lex::TokenIndex current_;
  [[maybe_unused]] llvm::raw_ostream* vlog_;
};

}  // namespace TinySwift::Parse

#endif  // TINYSWIFT_TOOLCHAIN_PARSE_CONTEXT_H_
