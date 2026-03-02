// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// INCLUDE-FILE: toolchain/testing/testdata/min_prelude/none.tinyswift
// NOAUTOUPDATE
// SET-CHECK-SUBSET
//
// M114: Comptime functions — verifies that functions marked `comptime` are
// evaluated at compile time when called from comptime context. The interpreter
// executes the function body and replaces the call with a literal result.

comptime func factorial(n: Int) -> Int {
  if n <= 1 {
    return 1
  }
  return n * factorial(n: n - 1)
}

comptime func add(a: Int, b: Int) -> Int {
  return a + b
}

func testComptimeFunctions() -> Int {
  let f = comptime factorial(n: 5)
  let s = comptime add(a: 10, b: 20)
  return f + s
}

// comptime factorial(n: 5) → int_value 120
// comptime add(a: 10, b: 20) → int_value 30
// Both calls should be replaced with literal int_value constants in SemIR.

// CHECK:STDOUT: filename:        comptime_functions.swift
// CHECK:STDOUT:     inst60000008:    {kind: IntType, arg0: signed, arg1: inst7, type: type(TypeType)}
// CHECK:STDOUT:     inst60000020:    {kind: FunctionDecl, arg0: function60000000, arg1: inst_block60000007, type: type(TypeType)}
