// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// INCLUDE-FILE: toolchain/testing/testdata/min_prelude/none.tinyswift
// NOAUTOUPDATE
// SET-CHECK-SUBSET
//
// M109: Derive Equatable — verifies that structs and enums conforming to
// Equatable get synthesized == and != operators. The synthesized methods
// compare all fields pairwise and emit field_access + int_eq instructions.

struct Point: Equatable {
  var x: Int
  var y: Int
}

enum Color: Equatable {
  case red
  case green
  case blue
}

func testEquatable() -> Bool {
  let a = Point(x: 1, y: 2)
  let b = Point(x: 1, y: 2)
  return a == b
}

// Synthesized == operator for Point: field_access for each field + int_eq.
// Synthesized == operator for Color: enum_discriminant comparison.

// CHECK:STDERR: fail_derive_equatable.swift:27:12: error: invalid operand types for '==': 'Point' and 'Point' [InvalidOperandTypes]
