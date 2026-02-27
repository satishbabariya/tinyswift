// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// INCLUDE-FILE: toolchain/testing/testdata/min_prelude/full.tinyswift
// NOAUTOUPDATE
// SET-CHECK-SUBSET
//
// Prelude integration test: Bool extensions (hashValue, description).

func testBoolExtensions() -> String {
  let b: Bool = true
  let h: Int = b.hashValue()
  let d: String = b.description()
  return d
}

// CHECK:STDOUT: FunctionDecl
// CHECK:STDOUT: Call
