// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// INCLUDE-FILE: toolchain/testing/testdata/min_prelude/none.tinyswift
// NOAUTOUPDATE
// SET-CHECK-SUBSET
//
// M75: @extern("C") LLVM IR linkage — verifies that @extern("C") functions
// are lowered to LLVM IR with external linkage and the unmangled C symbol name.
// The function should appear as a `declare` (not `define`) since it has no body.

@extern("C") func abs(_ x: Int) -> Int

func testExternC() -> Int {
  return abs(0 - 42)
}

// In LLVM IR:
//   - abs should be declared (not defined) with external linkage
//   - testExternC should be defined and call @abs

// CHECK:STDOUT: declare i64 @abs(i64)
// CHECK:STDOUT: define i64 @testExternC() !dbg !4 {
