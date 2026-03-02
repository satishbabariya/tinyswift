// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// INCLUDE-FILE: toolchain/testing/testdata/min_prelude/full.tinyswift
// NOAUTOUPDATE
// SET-CHECK-SUBSET
//
// Prelude integration test: Free functions (min, max, abs for Int and Double).

func testFreeFunctions() -> Int {
  let a: Int = min(3, 7)
  let b: Int = max(3, 7)
  let c: Int = abs(-5)
  let d: Double = min(1.5, 2.5)
  let e: Double = max(1.5, 2.5)
  let f: Double = abs(-3.14)
  return a + b + c
}

// CHECK:STDERR: fail_prelude_free_functions.swift:12:16: error: use of undefined name 'min' [UndefinedName]
