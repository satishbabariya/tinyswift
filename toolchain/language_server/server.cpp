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
#include "toolchain/lex/token_kind.h"
#include "toolchain/parse/parse.h"
#include "toolchain/parse/tree_and_subtrees.h"
#include "toolchain/sem_ir/file.h"
#include "toolchain/sem_ir/ids.h"
#include "toolchain/sem_ir/typed_insts.h"
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
  } else if (m == "textDocument/definition") {
    HandleDefinition(msg);
  } else if (m == "textDocument/hover") {
    HandleHover(msg);
  } else if (m == "textDocument/completion") {
    HandleCompletion(msg);
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
                              {"definitionProvider", true},
                              {"hoverProvider", true},
                              {"completionProvider",
                               llvm::json::Object{
                                   {"triggerCharacters",
                                    llvm::json::Array{"."}},
                               }},
                          }},
                         {"serverInfo",
                          llvm::json::Object{
                              {"name", "tinyswift-lsp"},
                              {"version", "0.2.0"},
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
  auto state = std::make_unique<DocumentState>();
  state->text = text.str();
  state->path = UriToPath(uri);

  // Create a source buffer from the editor content.
  auto source =
      SourceBuffer::MakeFromStringCopy(state->path, text, state->collector);
  if (!source) {
    // If we can't even create the buffer, publish empty diagnostics.
    documents_[uri] = std::move(state);
    SendNotification("textDocument/publishDiagnostics",
                     llvm::json::Object{
                         {"uri", uri},
                         {"diagnostics", llvm::json::Array{}},
                     });
    return;
  }
  state->source.emplace(std::move(*source));

  // Lex.
  state->value_stores = std::make_unique<SharedValueStores>();
  Lex::LexOptions lex_options;
  lex_options.consumer = &state->collector;
  state->tokens.emplace(
      Lex::Lex(*state->value_stores, *state->source, lex_options));

  // Parse.
  Parse::ParseOptions parse_options;
  parse_options.consumer = &state->collector;
  state->parse_tree.emplace(Parse::Parse(*state->tokens, parse_options));

  // Check.
  state->llvm_context = std::make_unique<llvm::LLVMContext>();
  state->sem_ir = std::make_unique<SemIR::File>(
      &*state->parse_tree, SemIR::CheckIRId(0), *state->value_stores,
      state->path);

  Check::Unit unit = {.consumer = &state->collector,
                      .value_stores = state->value_stores.get(),
                      .timings = nullptr,
                      .sem_ir = state->sem_ir.get(),
                      .llvm_context = state->llvm_context.get(),
                      .total_ir_count = 1};
  llvm::SmallVector<Check::Unit> units;
  units.push_back(unit);

  // Set up the tree-and-subtrees getter.
  // Capture raw pointers since the lambda only lives through CheckParseTrees.
  Lex::TokenizedBuffer* tokens_ptr = &*state->tokens;
  Parse::Tree* tree_ptr = &*state->parse_tree;
  std::optional<Parse::TreeAndSubtrees> tree_and_subtrees;
  auto getter = [tokens_ptr, tree_ptr, &tree_and_subtrees]()
      -> const Parse::TreeAndSubtrees& {
    if (!tree_and_subtrees) {
      tree_and_subtrees.emplace(*tokens_ptr, *tree_ptr);
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
  for (const auto& diag : state->collector.collected()) {
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

  // Store the state before sending (uri StringRef may alias documents_ key).
  std::string uri_str = uri.str();
  documents_[uri_str] = std::move(state);

  SendNotification("textDocument/publishDiagnostics",
                   llvm::json::Object{
                       {"uri", uri_str},
                       {"diagnostics", std::move(diag_array)},
                   });
}

// --- Utility Functions ---

auto Server::FindTokenAtPosition(const Lex::TokenizedBuffer& tokens,
                                 int line_0based, int col_0based)
    -> std::optional<Lex::TokenIndex> {
  std::optional<Lex::TokenIndex> closest;
  int closest_distance = std::numeric_limits<int>::max();

  for (auto token : tokens.tokens()) {
    auto kind = tokens.GetKind(token);
    if (kind == Lex::TokenKind::FileEnd) {
      continue;
    }

    int tok_line = tokens.GetLineNumber(token) - 1;  // to 0-based
    if (tok_line != line_0based) {
      continue;
    }

    int tok_col = tokens.GetColumnNumber(token) - 1;  // to 0-based
    int tok_len =
        static_cast<int>(tokens.GetTokenText(token).size());

    // Exact hit: cursor falls within [tok_col, tok_col + tok_len).
    if (col_0based >= tok_col && col_0based < tok_col + tok_len) {
      return token;
    }

    // Track closest token on the same line as fallback.
    int dist = (col_0based < tok_col) ? (tok_col - col_0based)
                                       : (col_0based - tok_col - tok_len + 1);
    if (dist < closest_distance) {
      closest_distance = dist;
      closest = token;
    }
  }

  return closest;
}

auto Server::FindInstAtToken(const SemIR::File& sem_ir,
                             const Parse::Tree& tree,
                             Lex::TokenIndex target_token)
    -> std::optional<SemIR::InstId> {
  std::optional<SemIR::InstId> result;

  for (auto [id, inst] : sem_ir.insts().enumerate()) {
    auto loc = sem_ir.insts().GetCanonicalLocId(id);
    if (loc.kind() != SemIR::LocId::Kind::NodeId) {
      continue;
    }
    auto node = loc.node_id();
    auto token = tree.node_token(node);
    if (token == target_token) {
      // Prefer NameRef if we find one (most useful for go-to-definition).
      if (inst.Is<SemIR::NameRef>()) {
        return id;
      }
      if (!result) {
        result = id;
      }
    }
  }

  return result;
}

auto Server::GetTypeDisplayName(const SemIR::File& file,
                                SemIR::TypeId type_id) -> std::string {
  if (!type_id.has_value()) {
    return "<type>";
  }

  auto inst = file.types().GetAsInst(type_id);

  if (inst.Is<SemIR::BoolType>()) {
    return "Bool";
  }
  if (inst.Is<SemIR::IntType>()) {
    return "Int";
  }
  if (inst.Is<SemIR::StringType>()) {
    return "String";
  }
  if (inst.Is<SemIR::FloatType>()) {
    return "Float";
  }
  if (inst.Is<SemIR::DoubleType>()) {
    return "Double";
  }
  if (inst.Is<SemIR::OptionalType>()) {
    return "Optional";
  }
  if (inst.Is<SemIR::FunctionType>()) {
    return "Function";
  }
  if (inst.Is<SemIR::PointerType>()) {
    return "Pointer";
  }
  if (inst.Is<SemIR::TupleType>()) {
    return "Tuple";
  }

  // Struct, class, or enum — look up the scope name.
  if (auto struct_type = inst.TryAs<SemIR::StructType>()) {
    auto scope_id = struct_type->name_scope_id;
    if (scope_id.has_value()) {
      const auto& scope = file.name_scopes().Get(scope_id);
      auto ident = scope.name_id.AsIdentifierId();
      if (ident.has_value()) {
        return file.identifiers().Get(ident).str();
      }
    }
    return "Struct";
  }
  if (auto class_type = inst.TryAs<SemIR::ClassType>()) {
    auto scope_id = class_type->name_scope_id;
    if (scope_id.has_value()) {
      const auto& scope = file.name_scopes().Get(scope_id);
      auto ident = scope.name_id.AsIdentifierId();
      if (ident.has_value()) {
        return file.identifiers().Get(ident).str();
      }
    }
    return "Class";
  }
  if (auto enum_decl = inst.TryAs<SemIR::EnumDecl>()) {
    auto scope_id = enum_decl->name_scope_id;
    if (scope_id.has_value()) {
      const auto& scope = file.name_scopes().Get(scope_id);
      auto ident = scope.name_id.AsIdentifierId();
      if (ident.has_value()) {
        return file.identifiers().Get(ident).str();
      }
    }
    return "Enum";
  }

  return "<type>";
}

auto Server::GetNameScopeForType(const SemIR::File& file,
                                 SemIR::TypeId type_id) -> SemIR::NameScopeId {
  if (!type_id.has_value()) {
    return SemIR::NameScopeId::None;
  }

  auto inst = file.types().GetAsInst(type_id);

  if (auto struct_type = inst.TryAs<SemIR::StructType>()) {
    return struct_type->name_scope_id;
  }
  if (auto class_type = inst.TryAs<SemIR::ClassType>()) {
    return class_type->name_scope_id;
  }
  if (auto enum_decl = inst.TryAs<SemIR::EnumDecl>()) {
    return enum_decl->name_scope_id;
  }

  return SemIR::NameScopeId::None;
}

// --- M85: Go-to-Definition ---

auto Server::HandleDefinition(const llvm::json::Object& msg) -> void {
  auto id = msg.get("id");
  if (!id) {
    return;
  }

  auto* params = msg.getObject("params");
  if (!params) {
    SendResponse(*id, nullptr);
    return;
  }

  auto* text_document = params->getObject("textDocument");
  auto* position = params->getObject("position");
  if (!text_document || !position) {
    SendResponse(*id, nullptr);
    return;
  }

  auto uri = text_document->getString("uri");
  if (!uri) {
    SendResponse(*id, nullptr);
    return;
  }

  auto it = documents_.find(*uri);
  if (it == documents_.end() || !it->second || !it->second->tokens ||
      !it->second->parse_tree || !it->second->sem_ir) {
    SendResponse(*id, nullptr);
    return;
  }

  auto& state = *it->second;
  int line = position->getInteger("line").value_or(0);
  int character = position->getInteger("character").value_or(0);

  // Find token at cursor.
  auto token = FindTokenAtPosition(*state.tokens, line, character);
  if (!token) {
    SendResponse(*id, nullptr);
    return;
  }

  // Find the SemIR instruction at this token.
  auto inst_id =
      FindInstAtToken(*state.sem_ir, *state.parse_tree, *token);
  if (!inst_id) {
    SendResponse(*id, nullptr);
    return;
  }

  // Resolve to the declaration.
  SemIR::InstId decl_id = *inst_id;
  auto inst = state.sem_ir->insts().Get(*inst_id);

  if (auto name_ref = inst.TryAs<SemIR::NameRef>()) {
    decl_id = name_ref->value_id;
  } else if (auto bound_method = inst.TryAs<SemIR::BoundMethod>()) {
    decl_id = bound_method->function_id;
  }

  if (!decl_id.has_value()) {
    SendResponse(*id, nullptr);
    return;
  }

  // Get the source location of the declaration.
  auto loc = state.sem_ir->insts().GetCanonicalLocId(decl_id);
  if (loc.kind() != SemIR::LocId::Kind::NodeId) {
    SendResponse(*id, nullptr);
    return;
  }

  auto node = loc.node_id();
  auto decl_token = state.parse_tree->node_token(node);
  int decl_line = state.tokens->GetLineNumber(decl_token) - 1;
  int decl_col = state.tokens->GetColumnNumber(decl_token) - 1;

  SendResponse(*id, llvm::json::Object{
                         {"uri", *uri},
                         {"range",
                          llvm::json::Object{
                              {"start",
                               llvm::json::Object{
                                   {"line", decl_line},
                                   {"character", decl_col},
                               }},
                              {"end",
                               llvm::json::Object{
                                   {"line", decl_line},
                                   {"character", decl_col},
                               }},
                          }},
                     });
}

// --- M86: Hover ---

auto Server::HandleHover(const llvm::json::Object& msg) -> void {
  auto id = msg.get("id");
  if (!id) {
    return;
  }

  auto* params = msg.getObject("params");
  if (!params) {
    SendResponse(*id, nullptr);
    return;
  }

  auto* text_document = params->getObject("textDocument");
  auto* position = params->getObject("position");
  if (!text_document || !position) {
    SendResponse(*id, nullptr);
    return;
  }

  auto uri = text_document->getString("uri");
  if (!uri) {
    SendResponse(*id, nullptr);
    return;
  }

  auto it = documents_.find(*uri);
  if (it == documents_.end() || !it->second || !it->second->tokens ||
      !it->second->parse_tree || !it->second->sem_ir) {
    SendResponse(*id, nullptr);
    return;
  }

  auto& state = *it->second;
  int line = position->getInteger("line").value_or(0);
  int character = position->getInteger("character").value_or(0);

  auto token = FindTokenAtPosition(*state.tokens, line, character);
  if (!token) {
    SendResponse(*id, nullptr);
    return;
  }

  auto inst_id =
      FindInstAtToken(*state.sem_ir, *state.parse_tree, *token);
  if (!inst_id) {
    SendResponse(*id, nullptr);
    return;
  }

  auto inst = state.sem_ir->insts().Get(*inst_id);
  std::string hover_text;

  // Helper to get a name string from a NameId.
  auto name_str = [&](SemIR::NameId name_id) -> std::string {
    if (!name_id.has_value()) {
      return "<unnamed>";
    }
    auto ident = name_id.AsIdentifierId();
    if (ident.has_value()) {
      return state.sem_ir->identifiers().Get(ident).str();
    }
    return "<special>";
  };

  if (auto name_ref = inst.TryAs<SemIR::NameRef>()) {
    // Look at what the name refers to.
    auto target_id = name_ref->value_id;
    if (target_id.has_value()) {
      auto target = state.sem_ir->insts().Get(target_id);

      if (auto fn_decl = target.TryAs<SemIR::FunctionDecl>()) {
        const auto& fn =
            state.sem_ir->functions().Get(fn_decl->function_id);
        hover_text = "func " + name_str(fn.name_id);
      } else if (target.Is<SemIR::VarStorage>()) {
        hover_text = "var " + name_str(name_ref->name_id) + ": " +
                     GetTypeDisplayName(*state.sem_ir, name_ref->type_id);
      } else if (target.Is<SemIR::ValueBinding>()) {
        hover_text = "let " + name_str(name_ref->name_id) + ": " +
                     GetTypeDisplayName(*state.sem_ir, name_ref->type_id);
      } else if (target.Is<SemIR::StructType>()) {
        hover_text = "struct " + name_str(name_ref->name_id);
      } else if (target.Is<SemIR::ClassType>()) {
        hover_text = "class " + name_str(name_ref->name_id);
      } else if (target.Is<SemIR::EnumDecl>()) {
        hover_text = "enum " + name_str(name_ref->name_id);
      } else {
        hover_text = name_str(name_ref->name_id) + ": " +
                     GetTypeDisplayName(*state.sem_ir, name_ref->type_id);
      }
    }
  } else if (auto fn_decl = inst.TryAs<SemIR::FunctionDecl>()) {
    const auto& fn = state.sem_ir->functions().Get(fn_decl->function_id);
    hover_text = "func " + name_str(fn.name_id);
  } else if (inst.Is<SemIR::VarStorage>()) {
    hover_text =
        "var: " + GetTypeDisplayName(*state.sem_ir, inst.type_id());
  } else if (auto val_param = inst.TryAs<SemIR::ValueParam>()) {
    hover_text = "param " + name_str(val_param->pretty_name_id) + ": " +
                 GetTypeDisplayName(*state.sem_ir, val_param->type_id);
  } else if (auto struct_type = inst.TryAs<SemIR::StructType>()) {
    auto scope_id = struct_type->name_scope_id;
    if (scope_id.has_value()) {
      const auto& scope = state.sem_ir->name_scopes().Get(scope_id);
      hover_text = "struct " + name_str(scope.name_id);
    } else {
      hover_text = "struct";
    }
  } else if (auto class_type = inst.TryAs<SemIR::ClassType>()) {
    auto scope_id = class_type->name_scope_id;
    if (scope_id.has_value()) {
      const auto& scope = state.sem_ir->name_scopes().Get(scope_id);
      hover_text = "class " + name_str(scope.name_id);
    } else {
      hover_text = "class";
    }
  } else {
    // Fallback: show the type.
    hover_text = GetTypeDisplayName(*state.sem_ir, inst.type_id());
  }

  if (hover_text.empty()) {
    SendResponse(*id, nullptr);
    return;
  }

  // Build token range for the hover highlight.
  int tok_line = state.tokens->GetLineNumber(*token) - 1;
  int tok_col = state.tokens->GetColumnNumber(*token) - 1;
  int tok_len =
      static_cast<int>(state.tokens->GetTokenText(*token).size());

  SendResponse(
      *id, llvm::json::Object{
               {"contents",
                llvm::json::Object{
                    {"kind", "markdown"},
                    {"value", "```swift\n" + hover_text + "\n```"},
                }},
               {"range",
                llvm::json::Object{
                    {"start",
                     llvm::json::Object{
                         {"line", tok_line},
                         {"character", tok_col},
                     }},
                    {"end",
                     llvm::json::Object{
                         {"line", tok_line},
                         {"character", tok_col + tok_len},
                     }},
                }},
           });
}

// --- M87: Autocomplete ---

auto Server::HandleCompletion(const llvm::json::Object& msg) -> void {
  auto id = msg.get("id");
  if (!id) {
    return;
  }

  auto* params = msg.getObject("params");
  if (!params) {
    SendResponse(*id, llvm::json::Object{
                           {"isIncomplete", false},
                           {"items", llvm::json::Array{}},
                       });
    return;
  }

  auto* text_document = params->getObject("textDocument");
  auto* position = params->getObject("position");
  if (!text_document || !position) {
    SendResponse(*id, llvm::json::Object{
                           {"isIncomplete", false},
                           {"items", llvm::json::Array{}},
                       });
    return;
  }

  auto uri = text_document->getString("uri");
  if (!uri) {
    SendResponse(*id, llvm::json::Object{
                           {"isIncomplete", false},
                           {"items", llvm::json::Array{}},
                       });
    return;
  }

  auto it = documents_.find(*uri);
  if (it == documents_.end() || !it->second || !it->second->tokens ||
      !it->second->parse_tree || !it->second->sem_ir) {
    SendResponse(*id, llvm::json::Object{
                           {"isIncomplete", false},
                           {"items", llvm::json::Array{}},
                       });
    return;
  }

  auto& state = *it->second;
  int line = position->getInteger("line").value_or(0);
  int character = position->getInteger("character").value_or(0);

  llvm::json::Array items;

  // Helper to get name string from NameId.
  auto name_str = [&](SemIR::NameId name_id) -> std::string {
    if (!name_id.has_value()) {
      return "";
    }
    auto ident = name_id.AsIdentifierId();
    if (ident.has_value()) {
      return state.sem_ir->identifiers().Get(ident).str();
    }
    return "";
  };

  // Determine completion kind by inspecting the token at/before cursor.
  auto token = FindTokenAtPosition(*state.tokens, line, character);

  bool is_dot_completion = false;
  std::optional<Lex::TokenIndex> before_dot_token;

  if (token) {
    auto kind = state.tokens->GetKind(*token);
    if (kind == Lex::TokenKind::Period) {
      is_dot_completion = true;
      // Find the token just before the period — walk backwards.
      for (auto t : state.tokens->tokens()) {
        if (t == *token) {
          break;
        }
        before_dot_token = t;
      }
    }
  }

  // Also check the token immediately before cursor for dot completion
  // (when cursor is right after the dot and we found an identifier after it).
  if (!is_dot_completion && character > 0) {
    auto prev_token =
        FindTokenAtPosition(*state.tokens, line, character - 1);
    if (prev_token) {
      auto kind = state.tokens->GetKind(*prev_token);
      if (kind == Lex::TokenKind::Period) {
        is_dot_completion = true;
        for (auto t : state.tokens->tokens()) {
          if (t == *prev_token) {
            break;
          }
          before_dot_token = t;
        }
      }
    }
  }

  if (is_dot_completion && before_dot_token) {
    // --- Dot completion: enumerate members of the type before the dot ---
    auto inst_id = FindInstAtToken(*state.sem_ir, *state.parse_tree,
                                   *before_dot_token);
    if (inst_id) {
      auto inst = state.sem_ir->insts().Get(*inst_id);
      SemIR::TypeId expr_type = inst.type_id();

      // If the expression is a NameRef, use the NameRef's type.
      if (auto name_ref = inst.TryAs<SemIR::NameRef>()) {
        expr_type = name_ref->type_id;
      }

      auto scope_id = GetNameScopeForType(*state.sem_ir, expr_type);
      if (scope_id.has_value()) {
        const auto& scope = state.sem_ir->name_scopes().Get(scope_id);
        for (const auto& [name_index, member_inst_id] : scope.names) {
          SemIR::NameId member_name_id(name_index);
          std::string member_name = name_str(member_name_id);
          if (member_name.empty()) {
            continue;
          }

          auto member_inst = state.sem_ir->insts().Get(member_inst_id);

          // Determine CompletionItemKind.
          // 2=Method, 3=Function, 5=Field, 6=Variable
          int kind = 6;  // Variable
          std::string detail;

          if (auto fn_decl =
                  member_inst.TryAs<SemIR::FunctionDecl>()) {
            const auto& fn =
                state.sem_ir->functions().Get(fn_decl->function_id);
            kind = fn.is_static ? 3 : 2;  // Function or Method
            detail = "func " + member_name;
          } else if (member_inst.Is<SemIR::VarStorage>()) {
            kind = 5;  // Field
            detail = GetTypeDisplayName(*state.sem_ir,
                                        member_inst.type_id());
          } else if (auto struct_field =
                         member_inst.TryAs<SemIR::StructField>()) {
            kind = 5;  // Field
            detail = GetTypeDisplayName(*state.sem_ir,
                                        struct_field->type_id);
          } else if (auto comp_prop =
                         member_inst
                             .TryAs<SemIR::ComputedPropertyDecl>()) {
            kind = 5;  // Field (computed property)
            detail = GetTypeDisplayName(*state.sem_ir,
                                        comp_prop->type_id);
          }

          llvm::json::Object item{
              {"label", member_name},
              {"kind", kind},
          };
          if (!detail.empty()) {
            item["detail"] = detail;
          }
          items.push_back(std::move(item));
        }
      }
    }
  } else {
    // --- Scope completion: top-level names + built-in types + keywords ---

    // Enumerate package scope (NameScopeId(0) = Package).
    if (state.sem_ir->name_scopes().Get(SemIR::NameScopeId::Package)
            .names.size() > 0) {
      const auto& package_scope =
          state.sem_ir->name_scopes().Get(SemIR::NameScopeId::Package);
      for (const auto& [name_index, decl_inst_id] : package_scope.names) {
        SemIR::NameId decl_name_id(name_index);
        std::string decl_name = name_str(decl_name_id);
        if (decl_name.empty()) {
          continue;
        }

        auto decl_inst = state.sem_ir->insts().Get(decl_inst_id);

        // Determine CompletionItemKind.
        int kind = 6;  // Variable
        if (decl_inst.Is<SemIR::FunctionDecl>()) {
          kind = 3;  // Function
        } else if (decl_inst.Is<SemIR::StructType>()) {
          kind = 22;  // Struct
        } else if (decl_inst.Is<SemIR::ClassType>()) {
          kind = 7;  // Class
        } else if (decl_inst.Is<SemIR::EnumDecl>()) {
          kind = 10;  // Enum
        }

        items.push_back(llvm::json::Object{
            {"label", decl_name},
            {"kind", kind},
        });
      }
    }

    // Add built-in types.
    for (const char* builtin :
         {"Bool", "Int", "String", "Float", "Double"}) {
      items.push_back(llvm::json::Object{
          {"label", builtin},
          {"kind", 22},  // Struct (type)
      });
    }

    // Add Swift keywords.
    for (const char* kw :
         {"func", "var", "let", "if", "else", "while", "for", "in",
          "return", "struct", "class", "enum", "switch", "case",
          "break", "continue", "guard", "defer", "repeat", "throw",
          "try", "catch", "true", "false", "nil", "self", "import",
          "protocol", "extension", "typealias", "static", "mutating",
          "init", "deinit", "inout", "print"}) {
      items.push_back(llvm::json::Object{
          {"label", kw},
          {"kind", 14},  // Keyword
      });
    }
  }

  SendResponse(*id, llvm::json::Object{
                         {"isIncomplete", false},
                         {"items", std::move(items)},
                     });
}

// --- Shared Infrastructure ---

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
