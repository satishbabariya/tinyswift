// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef TINYSWIFT_TOOLCHAIN_TINY_SIL_PRINTER_H_
#define TINYSWIFT_TOOLCHAIN_TINY_SIL_PRINTER_H_

#include "llvm/Support/raw_ostream.h"
#include "toolchain/tiny_sil/module.h"

namespace TinySwift::TinySIL {

// Prints a SIL module in textual form.
auto PrintSILModule(llvm::raw_ostream& out, const SILModule& module) -> void;

// Prints a single SIL function.
auto PrintSILFunction(llvm::raw_ostream& out, const SILFunction& function)
    -> void;

}  // namespace TinySwift::TinySIL

#endif  // TINYSWIFT_TOOLCHAIN_TINY_SIL_PRINTER_H_
