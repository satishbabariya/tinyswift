// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "toolchain/check/check.h"

#include "common/vlog.h"
#include "toolchain/sem_ir/file.h"

// TODO: Implement your language's semantic checking here.
// See TinySwift compiler for reference implementation patterns.
//
// The checker walks the parse tree and builds the SemIR representation.
// Key responsibilities include:
// - Name resolution and lookup
// - Type checking and inference
// - Building SemIR instructions from parse nodes
// - Constant evaluation

namespace TinySwift::Check {

auto CheckParseTrees(
    llvm::MutableArrayRef<Unit> units,
    const Parse::GetTreeAndSubtreesStore& /*tree_and_subtrees_getters*/,
    llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem> /*fs*/,
    const CheckParseTreesOptions& options,
    std::shared_ptr<clang::CompilerInvocation> /*clang_invocation*/) -> void {
  for (auto& unit : units) {
    TINYSWIFT_VLOG_TO(options.vlog_stream, "*** Checking: {0} ***\n",
                   unit.sem_ir->filename());

    // TODO: Walk the parse tree and build the SemIR.
    // For each parse node, create corresponding SemIR instructions.
    //
    // Example implementation steps:
    // 1. Initialize a checking context
    // 2. Traverse the parse tree in order
    // 3. Handle each node kind (function declarations, expressions, etc.)
    // 4. Perform name resolution
    // 5. Type check expressions
    // 6. Build SemIR instructions

    // Mark as having errors since we haven't actually checked anything.
    unit.sem_ir->set_has_errors(true);

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
