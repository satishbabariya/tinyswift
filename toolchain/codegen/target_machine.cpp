// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "toolchain/codegen/target_machine.h"

#include "llvm/MC/TargetRegistry.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/TargetParser/Triple.h"

namespace TinySwift {

auto MakeTargetMachine(const llvm::Target* target,
                       llvm::StringRef triple_str,
                       llvm::Module& module)
    -> std::unique_ptr<llvm::TargetMachine> {
  llvm::Triple target_triple(triple_str);
  module.setTargetTriple(target_triple);

  constexpr llvm::StringLiteral CPU = "generic";
  constexpr llvm::StringLiteral Features = "";

  llvm::TargetOptions target_opts;
  // Enable per-function/per-data sections for dead code stripping.
  target_opts.FunctionSections = true;
  target_opts.DataSections = true;
  return std::unique_ptr<llvm::TargetMachine>(target->createTargetMachine(
      target_triple, CPU, Features, target_opts, llvm::Reloc::PIC_));
}

}  // namespace TinySwift
