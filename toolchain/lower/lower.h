// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef TINYSWIFT_TOOLCHAIN_LOWER_LOWER_H_
#define TINYSWIFT_TOOLCHAIN_LOWER_LOWER_H_

#include "llvm/ADT/ArrayRef.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "toolchain/lower/options.h"
#include "toolchain/sem_ir/file.h"

// TODO: Implement your language's SemIR-to-LLVM-IR lowering here.
// See TinySwift compiler for reference implementation patterns.

namespace TinySwift::Lower {

// Lowers SemIR to LLVM IR.
auto LowerToLLVM(llvm::LLVMContext& llvm_context,
                 llvm::StringRef module_name,
                 const SemIR::File& sem_ir,
                 const LowerToLLVMOptions& options)
    -> std::unique_ptr<llvm::Module>;

}  // namespace TinySwift::Lower

#endif  // TINYSWIFT_TOOLCHAIN_LOWER_LOWER_H_
