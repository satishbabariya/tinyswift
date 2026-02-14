// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "toolchain/lower/lower.h"

#include <memory>

#include "common/vlog.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Verifier.h"

// TODO: Implement your language's SemIR-to-LLVM-IR lowering here.
// See TinySwift compiler for reference implementation patterns.
//
// The lowerer walks the SemIR and builds LLVM IR instructions.
// Key responsibilities include:
// - Creating LLVM functions from SemIR function definitions
// - Lowering SemIR types to LLVM types
// - Lowering SemIR instructions to LLVM IR instructions
// - Handling control flow (branches, returns)

namespace TinySwift::Lower {

auto LowerToLLVM(llvm::LLVMContext& llvm_context,
                 llvm::StringRef module_name,
                 const SemIR::File& sem_ir,
                 const LowerToLLVMOptions& options)
    -> std::unique_ptr<llvm::Module> {
  auto module = std::make_unique<llvm::Module>(module_name, llvm_context);

  TINYSWIFT_VLOG_TO(options.vlog_stream, "*** Lowering: {0} ***\n",
                 sem_ir.filename());

  // TODO: Walk the SemIR and build LLVM IR.
  // For each SemIR function, create an LLVM function and lower its body.
  //
  // Example implementation steps:
  // 1. Lower all SemIR types to LLVM types
  // 2. Create LLVM function declarations for all SemIR functions
  // 3. For each function with a body:
  //    a. Create basic blocks for each SemIR block
  //    b. Lower each SemIR instruction to LLVM IR
  //    c. Handle branches and returns
  // 4. Run the LLVM verifier on the result

  if (options.vlog_stream) {
    TINYSWIFT_VLOG_TO(options.vlog_stream, "*** llvm::Module ***\n");
    module->print(*options.vlog_stream, /*AAW=*/nullptr,
                  /*ShouldPreserveUseListOrder=*/false,
                  /*IsForDebug=*/true);
  }

  if (options.llvm_verifier_stream) {
    TINYSWIFT_CHECK(!llvm::verifyModule(*module, options.llvm_verifier_stream));
  }

  return module;
}

}  // namespace TinySwift::Lower
