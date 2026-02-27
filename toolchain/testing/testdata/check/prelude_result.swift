// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// INCLUDE-FILE: toolchain/testing/testdata/min_prelude/full.tinyswift
// NOAUTOUPDATE
// SET-CHECK-SUBSET
//
// Prelude integration test: Result get, map, flatMap, mapFailure.

func testResult() -> Int {
  let r: Result<Int, String> = .success(42)
  let v: Int = r.get()
  let mapped: Result<String, String> = r.map { (x: Int) -> String in return "ok" }
  return v
}

// CHECK:STDOUT: FunctionDecl
// CHECK:STDOUT: Call
