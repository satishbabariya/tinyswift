// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// ARGS: compile --phase=check --no-prelude-import %s
// NOAUTOUPDATE
// SET-CHECK-SUBSET
//
// Tests that the driver properly reports errors for invalid code and exits
// with failure status.

func broken() -> Int {
  let x: String = 42
  return x
}

// CHECK:STDERR: error
