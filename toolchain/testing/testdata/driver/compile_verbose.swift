// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// ARGS: -v compile --phase=check --no-prelude-import %s
// NOAUTOUPDATE
// SET-CHECK-SUBSET
//
// Tests that verbose mode (-v) produces logging output during compilation.

func main() -> Int {
  return 0
}

// CHECK:STDERR: ***
