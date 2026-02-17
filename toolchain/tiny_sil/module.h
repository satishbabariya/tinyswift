// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef TINYSWIFT_TOOLCHAIN_TINY_SIL_MODULE_H_
#define TINYSWIFT_TOOLCHAIN_TINY_SIL_MODULE_H_

#include <memory>
#include <string>

#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "toolchain/tiny_sil/function.h"

namespace TinySwift::TinySIL {

// A global variable in SIL.
struct SILGlobalVariable {
  std::string name;
  SILType type;
  bool is_let = false;  // true for let, false for var.
};

// The top-level SIL module — collection of functions and globals.
struct SILModule {
  std::string name;

  // All functions in the module.
  llvm::SmallVector<std::unique_ptr<SILFunction>> functions;

  // Global variables.
  llvm::SmallVector<std::unique_ptr<SILGlobalVariable>> globals;

  // Creates and adds a new function.
  auto createFunction(llvm::StringRef func_name) -> SILFunction* {
    auto fn = std::make_unique<SILFunction>();
    fn->name = std::string(func_name);
    auto* ptr = fn.get();
    functions.push_back(std::move(fn));
    return ptr;
  }

  // Finds a function by name.
  auto findFunction(llvm::StringRef func_name) const -> SILFunction* {
    for (auto& fn : functions) {
      if (fn->name == func_name) {
        return fn.get();
      }
    }
    return nullptr;
  }
};

}  // namespace TinySwift::TinySIL

#endif  // TINYSWIFT_TOOLCHAIN_TINY_SIL_MODULE_H_
