// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef TINYSWIFT_TOOLCHAIN_CODEGEN_TARGET_MACHINE_H_
#define TINYSWIFT_TOOLCHAIN_CODEGEN_TARGET_MACHINE_H_

#include <memory>

#include "llvm/ADT/StringRef.h"
#include "llvm/IR/Module.h"
#include "llvm/Target/TargetMachine.h"

namespace TinySwift {

// Creates an llvm::TargetMachine for code generation.
//
// Sets the module's target triple and configures function/data sections for
// dead code stripping. The returned machine uses PIC relocation and generic
// CPU settings.
auto MakeTargetMachine(const llvm::Target* target,
                       llvm::StringRef triple,
                       llvm::Module& module)
    -> std::unique_ptr<llvm::TargetMachine>;

}  // namespace TinySwift

#endif  // TINYSWIFT_TOOLCHAIN_CODEGEN_TARGET_MACHINE_H_
