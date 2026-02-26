// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "toolchain/language_server/server.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "llvm/IR/LLVMContext.h"
#include "toolchain/base/shared_value_stores.h"
#include "toolchain/check/check.h"
#include "toolchain/lex/lex.h"
#include "toolchain/parse/parse.h"
#include "toolchain/parse/tree_and_subtrees.h"
#include "toolchain/sem_ir/file.h"
#include "toolchain/sem_ir/ids.h"
#include "toolchain/source/source_buffer.h"

namespace TinySwift::LanguageServer {

// --- DiagnosticCollector ---

auto DiagnosticCollector::HandleDiagnostic(Diagnostics::Diagnostic diagnostic)
    -> void {
  for (const auto& message : diagnostic.messages) {
    // Only collect errors and warnings (skip notes, location info).
    if (message.level != Diagnostics::Level::Error &&
        message.level != Diagnostics::Level::Warning) {
      continue;
    }

    CollectedDiagnostic collected;
    collected.filename = message.loc.filename.str();

    // Convert from 1-based to 0-based. Use 0 if unknown (-1).
    collected.line =
        message.loc.line_number > 0 ? message.loc.line_number - 1 : 0;
    collected.column =
        message.loc.column_number > 0 ? message.loc.column_number - 1 : 0;
    collected.length = message.loc.length > 0 ? message.loc.length : 1;

    collected.severity =
        message.level == Diagnostics::Level::Error ? 1 : 2;

    collected.message = message.Format();
    collected_.push_back(std::move(collected));
  }
}

// --- Server ---

Server::Server(Transport& transport, llvm::raw_ostream& error)
    : transport_(transport), error_(error) {}

auto Server::Run() -> bool {
  while (true) {
    auto message = transport_.ReadMessage();
    if (!message) {
      // EOF or read error. If shutdown was requested, this is a clean exit
      // (the exit notification may or may not arrive).
      return shutdown_requested_;
    }

    auto* obj = message->getAsObject();
    if (!obj) {
      error_ << "LSP: received non-object message\n";
      continue;
    }

    HandleMessage(*obj);

    // Check for exit after handling.
    auto method = obj->getString("method");
    if (method && *method == "exit") {
      return shutdown_requested_;
    }
  }
}

auto Server::HandleMessage(const llvm::json::Object& msg) -> void {
  auto method = msg.getString("method");
  if (!method) {
    // Response or unknown message; ignore.
    return;
  }

  llvm::StringRef m = *method;
  if (m == "initialize") {
    HandleInitialize(msg);
  } else if (m == "initialized") {
    HandleInitialized();
  } else if (m == "textDocument/didOpen") {
    HandleDidOpen(msg);
  } else if (m == "textDocument/didChange") {
    HandleDidChange(msg);
  } else if (m == "textDocument/didClose") {
    HandleDidClose(msg);
  } else if (m == "shutdown") {
    HandleShutdown(msg);
  } else if (m == "exit") {
    // Handled in Run() after this returns.
  } else {
    // Unknown method. If it has an id, send a MethodNotFound error.
    auto id = msg.get("id");
    if (id) {
      transport_.WriteMessage(llvm::json::Object{
          {"jsonrpc", "2.0"},
          {"id", *id},
          {"error", llvm::json::Object{
                        {"code", -32601},
                        {"message", "method not found"},
                    }},
      });
    }
  }
}

auto Server::HandleInitialize(const llvm::json::Object& msg) -> void {
  auto id = msg.get("id");
  if (!id) {
    return;
  }

  // Respond with server capabilities.
  SendResponse(*id, llvm::json::Object{
                         {"capabilities",
                          llvm::json::Object{
                              // Full document sync: client sends entire text.
                              {"textDocumentSync", 1},
                          }},
                         {"serverInfo",
                          llvm::json::Object{
                              {"name", "tinyswift-lsp"},
                              {"version", "0.1.0"},
                          }},
                     });
}

auto Server::HandleInitialized() -> void {
  // No-op. The client is ready.
}

auto Server::HandleDidOpen(const llvm::json::Object& msg) -> void {
  auto* params = msg.getObject("params");
  if (!params) {
    return;
  }
  auto* text_document = params->getObject("textDocument");
  if (!text_document) {
    return;
  }

  auto uri = text_document->getString("uri");
  auto text = text_document->getString("text");
  if (!uri || !text) {
    return;
  }

  documents_[*uri] = text->str();
  CompileAndPublishDiagnostics(*uri, *text);
}

auto Server::HandleDidChange(const llvm::json::Object& msg) -> void {
  auto* params = msg.getObject("params");
  if (!params) {
    return;
  }
  auto* text_document = params->getObject("textDocument");
  if (!text_document) {
    return;
  }

  auto uri = text_document->getString("uri");
  if (!uri) {
    return;
  }

  // With full sync (textDocumentSync: 1), contentChanges[0].text is the
  // entire new content.
  auto* changes = params->getArray("contentChanges");
  if (!changes || changes->empty()) {
    return;
  }

  auto* first_change = (*changes)[0].getAsObject();
  if (!first_change) {
    return;
  }

  auto text = first_change->getString("text");
  if (!text) {
    return;
  }

  documents_[*uri] = text->str();
  CompileAndPublishDiagnostics(*uri, *text);
}

auto Server::HandleDidClose(const llvm::json::Object& msg) -> void {
  auto* params = msg.getObject("params");
  if (!params) {
    return;
  }
  auto* text_document = params->getObject("textDocument");
  if (!text_document) {
    return;
  }

  auto uri = text_document->getString("uri");
  if (!uri) {
    return;
  }

  documents_.erase(*uri);

  // Send empty diagnostics to clear any existing ones.
  SendNotification("textDocument/publishDiagnostics",
                   llvm::json::Object{
                       {"uri", *uri},
                       {"diagnostics", llvm::json::Array{}},
                   });
}

auto Server::HandleShutdown(const llvm::json::Object& msg) -> void {
  shutdown_requested_ = true;
  auto id = msg.get("id");
  if (id) {
    SendResponse(*id, nullptr);
  }
}

auto Server::CompileAndPublishDiagnostics(llvm::StringRef uri,
                                          llvm::StringRef text) -> void {
  std::string path = UriToPath(uri);

  // Set up a diagnostic collector.
  DiagnosticCollector collector;

  // Create a source buffer from the editor content.
  auto source = SourceBuffer::MakeFromStringCopy(path, text, collector);
  if (!source) {
    // If we can't even create the buffer, publish empty diagnostics.
    SendNotification("textDocument/publishDiagnostics",
                     llvm::json::Object{
                         {"uri", uri},
                         {"diagnostics", llvm::json::Array{}},
                     });
    return;
  }

  // Lex.
  SharedValueStores value_stores;
  Lex::LexOptions lex_options;
  lex_options.consumer = &collector;
  auto tokens = Lex::Lex(value_stores, *source, lex_options);

  // Parse.
  Parse::ParseOptions parse_options;
  parse_options.consumer = &collector;
  auto parse_tree = Parse::Parse(tokens, parse_options);

  // Check.
  auto llvm_context = std::make_unique<llvm::LLVMContext>();
  SemIR::File sem_ir(&parse_tree, SemIR::CheckIRId(0), value_stores, path);

  Check::Unit unit = {.consumer = &collector,
                      .value_stores = &value_stores,
                      .timings = nullptr,
                      .sem_ir = &sem_ir,
                      .llvm_context = llvm_context.get(),
                      .total_ir_count = 1};
  llvm::SmallVector<Check::Unit> units;
  units.push_back(unit);

  // Set up the tree-and-subtrees getter.
  std::optional<Parse::TreeAndSubtrees> tree_and_subtrees;
  auto getter = [&]() -> const Parse::TreeAndSubtrees& {
    if (!tree_and_subtrees) {
      tree_and_subtrees.emplace(tokens, parse_tree);
    }
    return *tree_and_subtrees;
  };
  std::function<auto()->const Parse::TreeAndSubtrees&> getter_fn = getter;

  auto tree_and_subtrees_store =
      Parse::GetTreeAndSubtreesStore::MakeWithExplicitSize(1, nullptr);
  tree_and_subtrees_store.Set(SemIR::CheckIRId(0),
                              Parse::GetTreeAndSubtreesFn(getter_fn));

  Check::CheckParseTreesOptions check_options;
  Check::CheckParseTrees(units, tree_and_subtrees_store, nullptr,
                         check_options, nullptr);

  // Convert collected diagnostics to LSP format.
  llvm::json::Array diag_array;
  for (const auto& diag : collector.collected()) {
    llvm::json::Object range{
        {"start", llvm::json::Object{
                      {"line", diag.line},
                      {"character", diag.column},
                  }},
        {"end", llvm::json::Object{
                    {"line", diag.line},
                    {"character", diag.column + diag.length},
                }},
    };

    diag_array.push_back(llvm::json::Object{
        {"range", std::move(range)},
        {"severity", diag.severity},
        {"source", "tinyswift"},
        {"message", diag.message},
    });
  }

  SendNotification("textDocument/publishDiagnostics",
                   llvm::json::Object{
                       {"uri", uri},
                       {"diagnostics", std::move(diag_array)},
                   });
}

auto Server::SendResponse(const llvm::json::Value& id,
                          llvm::json::Value result) -> void {
  transport_.WriteMessage(llvm::json::Object{
      {"jsonrpc", "2.0"},
      {"id", id},
      {"result", std::move(result)},
  });
}

auto Server::SendNotification(llvm::StringRef method,
                              llvm::json::Value params) -> void {
  transport_.WriteMessage(llvm::json::Object{
      {"jsonrpc", "2.0"},
      {"method", method},
      {"params", std::move(params)},
  });
}

auto Server::UriToPath(llvm::StringRef uri) -> std::string {
  // Strip the "file://" prefix.
  if (uri.consume_front("file://")) {
    return uri.str();
  }
  return uri.str();
}

}  // namespace TinySwift::LanguageServer
