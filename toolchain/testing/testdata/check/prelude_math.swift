// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// INCLUDE-FILE: toolchain/testing/testdata/min_prelude/full.tinyswift
// NOAUTOUPDATE
// SET-CHECK-SUBSET
//
// Prelude integration test: Math free functions (sqrt, pow, sin, cos, etc).

func testMath() -> Double {
  let a: Double = sqrt(4.0)
  let b: Double = pow(2.0, 10.0)
  let c: Double = sin(0.0)
  let d: Double = cos(0.0)
  let e: Double = floor(3.7)
  let f: Double = ceil(3.2)
  let g: Double = round(3.5)
  return a + b
}

// CHECK:STDOUT: FunctionDecl
// CHECK:STDOUT: Call
