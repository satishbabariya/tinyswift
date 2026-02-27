// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef TINYSWIFT_TOOLCHAIN_CODEGEN_OPTIMIZE_H_
#define TINYSWIFT_TOOLCHAIN_CODEGEN_OPTIMIZE_H_

#include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetMachine.h"
#include "toolchain/lower/options.h"

namespace TinySwift {

// Runs the LLVM optimization pipeline on the given module.
//
// `vlog_stream` may be null; when non-null the pipeline structure and
// optimized IR are printed to it.
auto RunLLVMOptimizePipeline(llvm::Module& module,
                             llvm::TargetMachine& target_machine,
                             Lower::OptimizationLevel opt_level,
                             llvm::raw_ostream* vlog_stream) -> void;

}  // namespace TinySwift

#endif  // TINYSWIFT_TOOLCHAIN_CODEGEN_OPTIMIZE_H_
