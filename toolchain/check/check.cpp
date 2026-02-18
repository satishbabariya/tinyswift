// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "toolchain/check/check.h"

#include <algorithm>

#include "common/vlog.h"
#include "toolchain/check/context.h"
#include "toolchain/check/handle_function.h"
#include "toolchain/check/handle_stmt.h"
#include "toolchain/check/handle_type_decl.h"
#include "toolchain/parse/node_kind.h"
#include "toolchain/parse/tree_and_subtrees.h"
#include "toolchain/sem_ir/file.h"
#include "toolchain/sem_ir/typed_insts.h"

namespace TinySwift::Check {

auto CheckParseTrees(
    llvm::MutableArrayRef<Unit> units,
    const Parse::GetTreeAndSubtreesStore& tree_and_subtrees_getters,
    llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem> /*fs*/,
    const CheckParseTreesOptions& options,
    std::shared_ptr<clang::CompilerInvocation> /*clang_invocation*/) -> void {
  for (auto& unit : units) {
    TINYSWIFT_VLOG_TO(options.vlog_stream, "*** Checking: {0} ***\n",
                   unit.sem_ir->filename());

    auto& tree_and_subtrees =
        tree_and_subtrees_getters.Get(unit.sem_ir->check_ir_id())();
    Context context(unit, tree_and_subtrees, *unit.consumer);

    // Create the package-level name scope and push it.
    auto package_scope_id = context.name_scopes().Add(
        SemIR::Namespace::PackageInstId, SemIR::NameId::None,
        SemIR::NameScopeId::None);
    context.PushScope(package_scope_id);

    // Push a top-level instruction block.
    context.PushInstBlock();

    // Collect top-level roots in source order.
    // tree_and_subtrees.roots() returns in RPO (reverse source order), so we
    // collect and reverse to get source order.
    llvm::SmallVector<Parse::NodeId> roots_vec;
    for (auto root : tree_and_subtrees.roots()) {
      roots_vec.push_back(root);
    }
    std::reverse(roots_vec.begin(), roots_vec.end());

    // Pass 1: Pre-register all top-level function and type names.
    // This enables forward references and mutual recursion across top-level
    // declarations (functions calling each other, types referencing each other).
    for (auto root : roots_vec) {
      auto kind = context.node_kind(root);
      if (kind == Parse::NodeKind::FunctionDefinition) {
        // Register the function's forward declaration (name in scope) without
        // processing the body. HandleFunctionDecl operates on the
        // FunctionDefinitionStart child which contains the signature.
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
      }
      // Note: struct/class/enum type names could be pre-registered here too
      // for mutual type references, but we defer that to when needed.
    }

    // Pass 2: Process all top-level declarations fully (including bodies).
    for (auto root : roots_vec) {
      auto kind = context.node_kind(root);

      // Skip FileStart and FileEnd.
      if (kind == Parse::NodeKind::FileStart ||
          kind == Parse::NodeKind::FileEnd) {
        continue;
      }

      HandleStatement(context, root);
    }

    // Pop the top-level block and set it as the file's top inst block.
    auto top_block_id = context.PopInstBlock();
    context.sem_ir().set_top_inst_block_id(top_block_id);

    context.PopScope();

    // Don't mark errors for successfully-checked files.
    // (The stub used to unconditionally set has_errors=true.)

    if (options.raw_dump_stream &&
        options.include_in_dumps &&
        options.include_in_dumps->Get(unit.sem_ir->check_ir_id())) {
      unit.sem_ir->Print(*options.raw_dump_stream,
                         options.dump_raw_sem_ir_builtins);
    }

    unit.consumer->Flush();
  }
}

}  // namespace TinySwift::Check
