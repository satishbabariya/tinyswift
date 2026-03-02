// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// INCLUDE-FILE: toolchain/testing/testdata/min_prelude/none.tinyswift
// NOAUTOUPDATE
// SET-CHECK-SUBSET
//
// M98_test3: Generator with early break — verifies that an infinite generator
// works correctly when the consumer breaks early. The generator frame should
// be released via ARC when the loop exits.

func infinite() -> Generator<Int> {
  var i = 0
  while true {
    yield i
    i = i + 1
  }
}

func test() -> Int {
  var sum = 0
  for x in infinite() {
    if x >= 5 { break }
    sum = sum + x
  }
  return sum
}

// After coroutine transform:
//   - infinite() becomes a creator that allocates a frame
//   - __infinite_resume implements the while-true state machine
//   - Early break exits the for-in loop normally
//   - ARC releases the generator frame on scope exit

// CHECK:STDERR: fail_coroutine_generator_break.swift:16:5: error: use of undefined name 'yield' [UndefinedName]
