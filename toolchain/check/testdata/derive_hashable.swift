// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// INCLUDE-FILE: toolchain/testing/testdata/min_prelude/none.tinyswift
// NOAUTOUPDATE
// SET-CHECK-SUBSET
//
// M110: Derive Hashable — verifies that structs and enums conforming to
// Hashable get a synthesized hash computed property. The property combines
// field hashes using multiplication and addition.

struct Pair: Hashable {
  var a: Int
  var b: Int
}

enum Direction: Hashable {
  case north
  case south
  case east
  case west
}

func testHash() -> Int {
  let p = Pair(a: 10, b: 20)
  return p.hash
}

// Synthesized hash property for Pair: field_access + int_mul + int_add.
// Synthesized hash property for Direction: enum_discriminant as hash basis.

// CHECK:STDOUT: StructType
// CHECK:STDOUT: FunctionDecl
// CHECK:STDOUT: FieldAccess
// CHECK:STDOUT: IntMul
// CHECK:STDOUT: IntAdd
// CHECK:STDOUT: EnumDecl
