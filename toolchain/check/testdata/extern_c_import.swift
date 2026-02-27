// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// INCLUDE-FILE: toolchain/testing/testdata/min_prelude/none.tinyswift
// NOAUTOUPDATE
// SET-CHECK-SUBSET
//
// M75: @extern("C") function import — verifies that @extern("C") declarations
// are parsed and produce SemIR function declarations with is_extern_c set.
// The function should be declaration-only (no body) and use the C symbol name.

@extern("C") func abs(_ x: Int) -> Int

func testExternC() -> Int {
  return abs(0 - 42)
}

// abs is an @extern("C") declaration — bodyless, with extern linkage.
// testExternC calls abs, which should appear as a regular call in SemIR.

// CHECK:STDOUT: FunctionDecl
// CHECK:STDOUT: FunctionDecl
// CHECK:STDOUT: call
