// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef TINYSWIFT_TOOLCHAIN_LANGUAGE_SERVER_SERVER_H_
#define TINYSWIFT_TOOLCHAIN_LANGUAGE_SERVER_SERVER_H_

#include <string>
#include <vector>

#include "llvm/ADT/StringMap.h"
#include "llvm/Support/JSON.h"
#include "toolchain/diagnostics/consumer.h"
#include "toolchain/language_server/transport.h"

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

  // Runs lex+parse+check on the given text and publishes diagnostics.
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

  Transport& transport_;
  llvm::raw_ostream& error_;

  // Open documents: URI -> content.
  llvm::StringMap<std::string> documents_;

  bool shutdown_requested_ = false;
};

}  // namespace TinySwift::LanguageServer

#endif  // TINYSWIFT_TOOLCHAIN_LANGUAGE_SERVER_SERVER_H_
