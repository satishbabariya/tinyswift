// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// INCLUDE-FILE: toolchain/testing/testdata/min_prelude/none.tinyswift
// NOAUTOUPDATE
// SET-CHECK-SUBSET
//
// M101_test1: Sequential await — verifies that an async function with multiple
// await points works correctly. Each await expression wraps an async call.
// In M100-M101, the async transform keeps the body as-is (synchronous mode)
// and await expressions are effectively pass-through wrappers.

func step1() async -> Int { return 10 }
func step2(_ n: Int) async -> Int { return n * 2 }

func pipeline() async -> Int {
  let a = await step1()
  let b = await step2(a)
  return b
}

func test() -> Int {
  return blockOn(pipeline())
}

// After check phase:
//   - step1, step2, pipeline all marked async (fn.is_async = true)
//   - pipeline body has two AwaitExpr instructions wrapping calls
//   - blockOn() unwraps the outermost async call
//   - In M100 synchronous mode, await is a no-op wrapper

// CHECK:STDOUT: filename:        coroutine_async_chaining.swift
// CHECK:STDOUT:     inst60000008:    {kind: IntType, arg0: signed, arg1: inst7, type: type(TypeType)}
// CHECK:STDOUT:     inst6000001D:    {kind: FunctionDecl, arg0: function60000000, arg1: inst_block60000005, type: type(TypeType)}
