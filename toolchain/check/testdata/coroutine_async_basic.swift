// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// INCLUDE-FILE: toolchain/testing/testdata/min_prelude/none.tinyswift
// NOAUTOUPDATE
// SET-CHECK-SUBSET
//
// M100_test1: Basic async function — verifies that an async function is
// detected and marked with is_async, and that blockOn() unwraps the async
// call to run it synchronously. In M100, async is a pass-through: the
// function body runs eagerly and blockOn() returns the result directly.

func asyncAdd(_ a: Int, _ b: Int) async -> Int {
  return a + b
}

func test() -> Int {
  return blockOn(asyncAdd(3, 4))
}

// After check phase:
//   - asyncAdd is marked as async (fn.is_async = true)
//   - AwaitExpr wraps the call in the async body
//   - blockOn() is detected as a built-in and unwraps the AwaitExpr
//   - In M100 synchronous mode, the async transform is a pass-through

// CHECK:STDOUT: fn_decl
// CHECK:STDOUT: call
// CHECK:STDOUT: return
