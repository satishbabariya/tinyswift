// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// ARGS: compile --phase=lex --dump-tokens --omit-file-boundary-tokens --no-prelude-import %s
// NOAUTOUPDATE
// SET-CHECK-SUBSET
//
// Tests that --dump-tokens produces lexer output for a simple function.

func greet() {
  let x: Int = 42
}

// CHECK:STDOUT: func
// CHECK:STDOUT: greet
// CHECK:STDOUT: let
// CHECK:STDOUT: Int
// CHECK:STDOUT: 42
