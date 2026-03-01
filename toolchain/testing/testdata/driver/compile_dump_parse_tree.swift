// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// ARGS: compile --phase=parse --dump-parse-tree --no-prelude-import %s
// NOAUTOUPDATE
// SET-CHECK-SUBSET
//
// Tests that --dump-parse-tree produces a valid parse tree output.

struct Point {
  var x: Int
  var y: Int
}

func main() -> Int {
  return 0
}

// CHECK:STDOUT: StructDecl
// CHECK:STDOUT: FunctionDecl
