// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef TINYSWIFT_TOOLCHAIN_LANGUAGE_SERVER_SERVER_H_
#define TINYSWIFT_TOOLCHAIN_LANGUAGE_SERVER_SERVER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "llvm/ADT/StringMap.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/Support/JSON.h"
#include "toolchain/base/shared_value_stores.h"
#include "toolchain/diagnostics/consumer.h"
#include "toolchain/language_server/transport.h"
#include "toolchain/lex/token_index.h"
#include "toolchain/lex/tokenized_buffer.h"
#include "toolchain/parse/tree.h"
#include "toolchain/sem_ir/file.h"
#include "toolchain/sem_ir/ids.h"
#include "toolchain/sem_ir/typed_insts.h"
#include "toolchain/source/source_buffer.h"

namespace TinySwift::LanguageServer {

// A diagnostic collected from a compilation pass, with 0-based positions
// suitable for LSP.
struct CollectedDiagnostic {
  std::string filename;
  int line;        // 0-based
  int column;      // 0-based
  int length;      // character count
  int severity;    // 1=Error, 2=Warning, 3=Information, 4=Hint
  std::string message;
};

// A diagnostic consumer that collects diagnostics into a vector instead of
// printing them.
class DiagnosticCollector : public Diagnostics::Consumer {
 public:
  auto HandleDiagnostic(Diagnostics::Diagnostic diagnostic) -> void override;

  auto collected() const -> const std::vector<CollectedDiagnostic>& {
    return collected_;
  }

  auto Clear() -> void { collected_.clear(); }

 private:
  std::vector<CollectedDiagnostic> collected_;
};

// Owns all compilation artifacts for an open document, keeping them alive
// for query handlers (definition, hover, completion).
struct DocumentState {
  std::string text;
  std::string path;
  DiagnosticCollector collector;
  std::optional<SourceBuffer> source;
  std::unique_ptr<SharedValueStores> value_stores;
  std::optional<Lex::TokenizedBuffer> tokens;
  std::optional<Parse::Tree> parse_tree;
  std::unique_ptr<llvm::LLVMContext> llvm_context;
  std::unique_ptr<SemIR::File> sem_ir;
};

// The LSP server. Reads JSON-RPC messages from the transport, handles them,
// and sends responses/notifications back.
class Server {
 public:
  explicit Server(Transport& transport, llvm::raw_ostream& error);

  // Runs the server message loop until shutdown/exit. Returns true on clean
  // exit.
  auto Run() -> bool;

 private:
  auto HandleMessage(const llvm::json::Object& msg) -> void;
  auto HandleInitialize(const llvm::json::Object& msg) -> void;
  auto HandleInitialized() -> void;
  auto HandleDidOpen(const llvm::json::Object& msg) -> void;
  auto HandleDidChange(const llvm::json::Object& msg) -> void;
  auto HandleDidClose(const llvm::json::Object& msg) -> void;
  auto HandleShutdown(const llvm::json::Object& msg) -> void;

  // M85: textDocument/definition
  auto HandleDefinition(const llvm::json::Object& msg) -> void;
  // M86: textDocument/hover
  auto HandleHover(const llvm::json::Object& msg) -> void;
  // M87: textDocument/completion
  auto HandleCompletion(const llvm::json::Object& msg) -> void;

  // Runs lex+parse+check on the given text and stores results in documents_.
  auto CompileAndPublishDiagnostics(llvm::StringRef uri,
                                    llvm::StringRef text) -> void;

  // Sends a JSON-RPC response for a request.
  auto SendResponse(const llvm::json::Value& id,
                    llvm::json::Value result) -> void;

  // Sends a JSON-RPC notification (no id).
  auto SendNotification(llvm::StringRef method,
                        llvm::json::Value params) -> void;

  // Converts a file:// URI to a filesystem path.
  static auto UriToPath(llvm::StringRef uri) -> std::string;

  // --- Utility functions for M85-M87 ---

  // Finds the token at the given 0-based line and column.
  static auto FindTokenAtPosition(const Lex::TokenizedBuffer& tokens,
                                  int line_0based, int col_0based)
      -> std::optional<Lex::TokenIndex>;

  // Finds a SemIR instruction whose source location matches the given token.
  static auto FindInstAtToken(const SemIR::File& sem_ir,
                              const Parse::Tree& tree,
                              Lex::TokenIndex target_token)
      -> std::optional<SemIR::InstId>;

  // Returns a human-readable name for a type.
  static auto GetTypeDisplayName(const SemIR::File& file, SemIR::TypeId type_id)
      -> std::string;

  // Returns the NameScopeId for a struct/class/enum type.
  static auto GetNameScopeForType(const SemIR::File& file,
                                  SemIR::TypeId type_id) -> SemIR::NameScopeId;

  Transport& transport_;
  llvm::raw_ostream& error_;

  // Open documents: URI -> DocumentState (owns all compilation artifacts).
  llvm::StringMap<std::unique_ptr<DocumentState>> documents_;

  bool shutdown_requested_ = false;
};

}  // namespace TinySwift::LanguageServer

#endif  // TINYSWIFT_TOOLCHAIN_LANGUAGE_SERVER_SERVER_H_
