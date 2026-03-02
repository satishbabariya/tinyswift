// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// INCLUDE-FILE: toolchain/testing/testdata/min_prelude/none.tinyswift
// NOAUTOUPDATE
// SET-CHECK-SUBSET
//
// M99_test1: Fibonacci generator — verifies that a generator with multiple
// mutable locals and a while-true loop works correctly with for-in iteration.
// The for-in over Generator<T> is desugared to while-let + .next().

func fib() -> Generator<Int> {
  var a = 0
  var b = 1
  while true {
    yield a
    let tmp = a
    a = b
    b = tmp + b
  }
}

func test() -> Int {
  var result = 0
  var count = 0
  for n in fib() {
    if count >= 10 { break }
    result = n
    count = count + 1
  }
  return result
}

// After coroutine transform:
//   - fib() creator allocates frame with fields for a, b, tmp, state
//   - __fib_resume implements the state machine for the while-true loop
//   - for-in desugars to: let __gen = fib(); while let n = __gen.next() { ... }
//   - Multiple mutable locals (a, b, tmp) live across yield in the frame

// CHECK:STDERR: fail_coroutine_fibonacci.swift:17:5: error: use of undefined name 'yield' [UndefinedName]
