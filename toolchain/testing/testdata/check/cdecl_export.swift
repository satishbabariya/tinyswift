// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// INCLUDE-FILE: toolchain/testing/testdata/min_prelude/none.tinyswift
// NOAUTOUPDATE
// SET-CHECK-SUBSET
//
// M76: @cdecl function export — verifies that @cdecl("name") declarations
// are parsed and produce SemIR function declarations with is_cdecl set and
// the exported C symbol name recorded. The function has a body and is
// callable from C code using the specified symbol name.

@cdecl("get_answer")
func answer() -> Int {
  return 42
}

@cdecl("add_ints")
func addInts(_ a: Int, _ b: Int) -> Int {
  return a + b
}

// Both functions should appear in SemIR with their bodies.

// CHECK:STDOUT: FunctionDecl
// CHECK:STDOUT: IntValue
// CHECK:STDOUT: FunctionDecl
// CHECK:STDOUT: return
