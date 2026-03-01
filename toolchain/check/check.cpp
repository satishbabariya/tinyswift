// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "toolchain/check/check.h"

#include <algorithm>

#include "common/vlog.h"
#include "toolchain/check/context.h"
#include "toolchain/check/coroutine_transform.h"
#include "toolchain/check/handle_function.h"
#include "toolchain/check/handle_stmt.h"
#include "toolchain/check/handle_type_decl.h"
#include "toolchain/parse/node_kind.h"
#include "toolchain/parse/tree_and_subtrees.h"
#include "toolchain/sem_ir/file.h"
#include "toolchain/sem_ir/typed_insts.h"

namespace TinySwift::Check {

// M73: Collects roots in source order for a given tree.
static auto CollectRootsSourceOrder(
    const Parse::TreeAndSubtrees& tree_and_subtrees)
    -> llvm::SmallVector<Parse::NodeId> {
  llvm::SmallVector<Parse::NodeId> roots_vec;
  for (auto root : tree_and_subtrees.roots()) {
    roots_vec.push_back(root);
  }
  std::reverse(roots_vec.begin(), roots_vec.end());
  return roots_vec;
}

auto CheckParseTrees(
    llvm::MutableArrayRef<Unit> units,
    const Parse::GetTreeAndSubtreesStore& tree_and_subtrees_getters,
    llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem> /*fs*/,
    const CheckParseTreesOptions& options,
    std::shared_ptr<clang::CompilerInvocation> /*clang_invocation*/) -> void {
  if (units.empty()) {
    return;
  }

  // M73: For multi-file compilation, all files share ONE Context using the
  // primary unit (units[0]). All SemIR accumulates into units[0].sem_ir.
  // Each pass runs across ALL files before advancing to the next pass.

  auto& primary_unit = units[0];
  TINYSWIFT_VLOG_TO(options.vlog_stream, "*** Checking (multi-file): {0} ***\n",
                 primary_unit.sem_ir->filename());

  auto& primary_tree_and_subtrees =
      tree_and_subtrees_getters.Get(primary_unit.sem_ir->check_ir_id())();
  Context context(primary_unit, primary_tree_and_subtrees, *primary_unit.consumer);
  context.SetAllTrees(&tree_and_subtrees_getters);

  // M107: Pass conditional compilation defines to context.
  if (!options.defines.empty()) {
    context.SetDefines(options.defines);
  }

  // Create the package-level name scope and push it.
  auto package_scope_id = context.name_scopes().Add(
      SemIR::Namespace::PackageInstId, SemIR::NameId::None,
      SemIR::NameScopeId::None);
  context.PushScope(package_scope_id);

  // M88: Register built-in type names in package scope so extensions can
  // resolve them via LookupName (e.g. `extension Int { ... }`).
  {
    // Singleton types with fixed InstIds.
    static const struct {
      const char* name;
      SemIR::InstId inst_id;
    } singleton_types[] = {
        {"String", SemIR::StringType::TypeInstId},
        {"Float", SemIR::FloatType::TypeInstId},
        {"Double", SemIR::DoubleType::TypeInstId},
    };
    for (const auto& bt : singleton_types) {
      auto ident_id = context.identifiers().Add(bt.name);
      auto name_id = SemIR::NameId::ForIdentifier(ident_id);
      context.AddNameToScope(name_id, bt.inst_id);
    }
    // Dynamic types (Bool=Int1, Int=Int64) — register via GetBuiltinType.
    static const char* dynamic_type_names[] = {"Bool", "Int", "UInt"};
    for (const auto* type_name : dynamic_type_names) {
      auto type_id = context.GetBuiltinType(type_name);
      auto type_inst_id = context.types().GetTypeInstId(type_id);
      auto ident_id = context.identifiers().Add(type_name);
      auto name_id = SemIR::NameId::ForIdentifier(ident_id);
      context.AddNameToScope(name_id, type_inst_id);
    }
  }

  // Push a top-level instruction block.
  context.PushInstBlock();

  // Collect roots for each file.
  struct FileRoots {
    SemIR::CheckIRId check_ir_id;
    llvm::SmallVector<Parse::NodeId> roots;
  };
  llvm::SmallVector<FileRoots> all_file_roots;

  for (auto& unit : units) {
    auto check_ir_id = unit.sem_ir->check_ir_id();
    auto& tree_and_subtrees = tree_and_subtrees_getters.Get(check_ir_id)();
    auto roots = CollectRootsSourceOrder(tree_and_subtrees);
    all_file_roots.push_back({check_ir_id, std::move(roots)});
  }

  // Track which roots were processed as type definitions or extensions,
  // keyed by (file_index, node_index).
  llvm::DenseSet<int64_t> processed_type_roots;
  auto make_key = [](int file_idx, uint32_t node_idx) -> int64_t {
    return (static_cast<int64_t>(file_idx) << 32) | node_idx;
  };

  // Helper: detect AccessModifier and set pending access level (M74).
  auto handle_access_modifier = [&](Context& ctx, Parse::NodeId root) -> bool {
    auto kind = ctx.node_kind(root);
    if (kind == Parse::NodeKind::AccessModifier) {
      auto token = ctx.node_token(root);
      auto text = ctx.token_text(token);
      if (text == "public") {
        ctx.SetPendingAccessLevel(SemIR::AccessLevel::Public);
      } else if (text == "private") {
        ctx.SetPendingAccessLevel(SemIR::AccessLevel::Private);
      } else {
        ctx.SetPendingAccessLevel(SemIR::AccessLevel::Internal);
      }
      return true;
    }
    // M75/M76: Store Attribute nodes for next declaration to consume.
    if (kind == Parse::NodeKind::Attribute) {
      ctx.SetPendingAttribute(root);
      return true;
    }
    // M112: ComptimeModifier before `func` declarations.
    if (kind == Parse::NodeKind::ComptimeModifier) {
      ctx.SetPendingComptimeHint(true);
      return true;
    }
    return false;
  };

  // --- Pass 1a: Process all top-level type definitions across ALL files ---
  for (int file_idx = 0; file_idx < static_cast<int>(all_file_roots.size()); ++file_idx) {
    auto& file_roots = all_file_roots[file_idx];
    auto& tree_and_subtrees = tree_and_subtrees_getters.Get(file_roots.check_ir_id)();
    context.SetCurrentTreeAndSubtrees(tree_and_subtrees);
    context.SetCurrentFile(file_roots.check_ir_id);
    // M89: Clear stale pending state from previous file to prevent cross-file
    // NodeId mismatches (attribute/access nodes belong to a specific parse tree).
    context.TakePendingAttribute();
    context.TakePendingAccessLevel();

    for (auto root : file_roots.roots) {
      if (handle_access_modifier(context, root)) continue;
      auto kind = context.node_kind(root);
      if (kind == Parse::NodeKind::EnumDefinition ||
          kind == Parse::NodeKind::StructDefinition ||
          kind == Parse::NodeKind::ClassDefinition ||
          kind == Parse::NodeKind::TypealiasDecl ||
          kind == Parse::NodeKind::ProtocolDefinition) {
        HandleStatement(context, root);
        processed_type_roots.insert(make_key(file_idx, root.index));
      }
    }
  }

  // --- Pass 1b: Pre-register all top-level function names across ALL files ---
  for (int file_idx = 0; file_idx < static_cast<int>(all_file_roots.size()); ++file_idx) {
    auto& file_roots = all_file_roots[file_idx];
    auto& tree_and_subtrees = tree_and_subtrees_getters.Get(file_roots.check_ir_id)();
    context.SetCurrentTreeAndSubtrees(tree_and_subtrees);
    context.SetCurrentFile(file_roots.check_ir_id);
    context.TakePendingAttribute();
    context.TakePendingAccessLevel();

    for (auto root : file_roots.roots) {
      if (handle_access_modifier(context, root)) continue;
      auto kind = context.node_kind(root);
      if (kind == Parse::NodeKind::FunctionDefinition) {
        auto children = context.children_source_order(root);
        for (auto child : children) {
          if (context.node_kind(child) ==
              Parse::NodeKind::FunctionDefinitionStart) {
            HandleFunctionDecl(context, child);
            break;
          }
        }
      } else if (kind == Parse::NodeKind::FunctionDecl) {
        HandleFunctionDecl(context, root);
        // Mark bodyless FunctionDecl as processed so Pass 2 skips it.
        // This prevents duplicate function entries for @extern declarations.
        processed_type_roots.insert(make_key(file_idx, root.index));
      }
    }
  }

  // --- Pass 1c: Process all top-level extensions across ALL files ---
  for (int file_idx = 0; file_idx < static_cast<int>(all_file_roots.size()); ++file_idx) {
    auto& file_roots = all_file_roots[file_idx];
    auto& tree_and_subtrees = tree_and_subtrees_getters.Get(file_roots.check_ir_id)();
    context.SetCurrentTreeAndSubtrees(tree_and_subtrees);
    context.SetCurrentFile(file_roots.check_ir_id);
    context.TakePendingAttribute();
    context.TakePendingAccessLevel();

    for (auto root : file_roots.roots) {
      if (handle_access_modifier(context, root)) continue;
      auto kind = context.node_kind(root);
      if (kind == Parse::NodeKind::ExtensionDefinition) {
        HandleStatement(context, root);
        processed_type_roots.insert(make_key(file_idx, root.index));
      }
    }
  }

  // --- Pass 2: Process all remaining top-level declarations across ALL files ---
  for (int file_idx = 0; file_idx < static_cast<int>(all_file_roots.size()); ++file_idx) {
    auto& file_roots = all_file_roots[file_idx];
    auto& tree_and_subtrees = tree_and_subtrees_getters.Get(file_roots.check_ir_id)();
    context.SetCurrentTreeAndSubtrees(tree_and_subtrees);
    context.SetCurrentFile(file_roots.check_ir_id);
    context.TakePendingAttribute();
    context.TakePendingAccessLevel();

    for (auto root : file_roots.roots) {
      if (handle_access_modifier(context, root)) continue;
      auto kind = context.node_kind(root);

      // Skip FileStart and FileEnd.
      if (kind == Parse::NodeKind::FileStart ||
          kind == Parse::NodeKind::FileEnd) {
        continue;
      }

      // Skip type definitions/extensions already processed.
      if (processed_type_roots.count(make_key(file_idx, root.index))) {
        continue;
      }

      HandleStatement(context, root);
    }
  }

  // --- Pass 3: Coroutine transform (M98-M102) ---
  // Rewrites generator and async functions into state machines.
  // After this pass, all Yield/AwaitExpr instructions are eliminated.
  TransformCoroutines(context);

  // Pop the top-level block and set it as the file's top inst block.
  auto top_block_id = context.PopInstBlock();
  context.sem_ir().set_top_inst_block_id(top_block_id);

  context.PopScope();

  // Dump/diagnostics on primary unit only.
  if (options.dump_stream &&
      options.include_in_dumps &&
      options.include_in_dumps->Get(primary_unit.sem_ir->check_ir_id())) {
    primary_unit.sem_ir->Print(*options.dump_stream);
  }
  if (options.raw_dump_stream &&
      options.include_in_dumps &&
      options.include_in_dumps->Get(primary_unit.sem_ir->check_ir_id())) {
    primary_unit.sem_ir->Print(*options.raw_dump_stream,
                               options.dump_raw_sem_ir_builtins);
  }

  primary_unit.consumer->Flush();

  // For non-primary units, mark their sem_ir as having no content (all SemIR
  // accumulated into primary_unit). Also flush their consumers.
  for (size_t i = 1; i < units.size(); ++i) {
    units[i].consumer->Flush();
  }
}

}  // namespace TinySwift::Check
