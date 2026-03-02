// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// INCLUDE-FILE: toolchain/testing/testdata/min_prelude/none.tinyswift
// NOAUTOUPDATE
// SET-CHECK-SUBSET
//
// M99_test2: Generator + ARC — verifies that class-typed variables inside a
// generator body are properly managed by ARC. The Box instance lives in the
// generator frame and is released when the frame is freed.

class Box {
  var value: Int
  init(value: Int) {
    self.value = value
  }
}

func boxes() -> Generator<Int> {
  let b = Box(value: 42)
  yield b.value
  yield b.value + 1
}

func test() -> Int {
  let gen = boxes()
  let a = gen.next()!
  let b = gen.next()!
  return a + b
}

// After coroutine transform:
//   - boxes() creator allocates frame, stores Box instance in frame field
//   - __boxes_resume reads Box from frame to access .value
//   - ARC retains/releases the Box through generator frame lifetime
//   - When generator frame is freed, Box is released (no memory leak)

// CHECK:STDERR: fail_coroutine_generator_arc.swift:22:3: error: use of undefined name 'yield' [UndefinedName]
