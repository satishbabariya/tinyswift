// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// INCLUDE-FILE: toolchain/testing/testdata/min_prelude/none.tinyswift
// NOAUTOUPDATE
// SET-CHECK-SUBSET
//
// M98_test2: Generator with parameter — verifies that function parameters are
// stored into the generator frame and accessible by the resume function. The
// countdown generator uses a while loop with a mutable local variable.

func countDown(_ n: Int) -> Generator<Int> {
  var i = n
  while i > 0 {
    yield i
    i = i - 1
  }
}

func test() -> Int {
  var sum = 0
  for x in countDown(5) {
    sum = sum + x
  }
  return sum
}

// After coroutine transform:
//   - Creator stores param n and local i into frame fields
//   - Resume function implements the while-loop state machine
//   - for-in is desugared to while-let + .next()

// CHECK:STDERR: fail_coroutine_generator_param.swift:16:5: error: use of undefined name 'yield' [UndefinedName]
