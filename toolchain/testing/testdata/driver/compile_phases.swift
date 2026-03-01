// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// ARGS: compile --phase=check --dump-sem-ir --dump-sem-ir-ranges=only --no-prelude-import %s
// NOAUTOUPDATE
// SET-CHECK-SUBSET
//
// Tests that the compile subcommand correctly handles phase selection,
// running through lex, parse, and check phases.

func add(_ a: Int, _ b: Int) -> Int {
  return a + b
}

func main() -> Int {
  let result: Int = add(3, 4)
  return result
}

// CHECK:STDOUT: FunctionDecl
// CHECK:STDOUT: FunctionDecl
