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
#include "toolchain/tiny_sil/module.h"

namespace TinySwift::Lower {

// Lowers SemIR directly to LLVM IR (legacy path).
auto LowerToLLVM(llvm::LLVMContext& llvm_context,
                 llvm::StringRef module_name,
                 const SemIR::File& sem_ir,
                 const LowerToLLVMOptions& options)
    -> std::unique_ptr<llvm::Module>;

// Lowers TinySIL to LLVM IR (new SIL-based path).
auto LowerSILToLLVM(llvm::LLVMContext& llvm_context,
                    llvm::StringRef module_name,
                    const TinySIL::SILModule& sil_module,
                    const SemIR::File& sem_ir,
                    const LowerToLLVMOptions& options)
    -> std::unique_ptr<llvm::Module>;

}  // namespace TinySwift::Lower

#endif  // TINYSWIFT_TOOLCHAIN_LOWER_LOWER_H_
