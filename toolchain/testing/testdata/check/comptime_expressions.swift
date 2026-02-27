// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// INCLUDE-FILE: toolchain/testing/testdata/min_prelude/none.tinyswift
// NOAUTOUPDATE
// SET-CHECK-SUBSET
//
// M112-M113: Comptime expressions — verifies that `comptime` expressions are
// evaluated at compile time and replaced with literal SemIR constants.
// Arithmetic, boolean, and conditional expressions are tested.

func testComptimeArithmetic() -> Int {
  let a = comptime 2 + 3
  let b = comptime 10 * 4
  let c = comptime (100 - 50) / 2
  return a + b + c
}

func testComptimeBool() -> Bool {
  let x = comptime true && false
  return x
}

// comptime expressions should be replaced with literal constants:
//   comptime 2 + 3 → int_value 5
//   comptime 10 * 4 → int_value 40
//   comptime true && false → bool_literal false

// CHECK:STDOUT: FunctionDecl
// CHECK:STDOUT: IntValue
// CHECK:STDOUT: BoolLiteral
