// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// ARGS: compile --phase=check --dump-sem-ir --dump-sem-ir-ranges=only --no-prelude-import --define=TESTING %s
// NOAUTOUPDATE
// SET-CHECK-SUBSET
//
// Tests that --define passes conditional compilation flags through the driver.

func main() -> Int {
  #if TESTING
  return 0
  #else
  return 1
  #endif
}

// CHECK:STDOUT: FunctionDecl
