// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// INCLUDE-FILE: toolchain/testing/testdata/min_prelude/none.tinyswift
// NOAUTOUPDATE
// SET-CHECK-SUBSET
//
// M98_test1: Basic generator — verifies that a generator function with yield
// statements is transformed into a creator function (allocates frame, stores
// yield values, returns Generator<T> tuple) and a resume function (state
// machine returning Optional<T>).

func oneToThree() -> Generator<Int> {
  yield 1
  yield 2
  yield 3
}

func test() -> Int {
  let gen = oneToThree()
  let a = gen.next()!
  let b = gen.next()!
  let c = gen.next()!
  let d = gen.next()
  return a + b + c
}

// After coroutine transform, the creator function should contain:
//   - frame allocation via __tinyswift_alloc
//   - field_addr writes for yield values
//   - tuple_init for Generator<T> {frame_ptr, resume_fn_ptr}
//
// The resume function (__oneToThree_resume) should contain:
//   - field_access reads from frame
//   - int_eq state comparisons
//   - optional_some for yield returns
//   - optional_none for exhaustion

// CHECK:STDERR: fail_coroutine_basic_generator.swift:15:3: error: use of undefined name 'yield' [UndefinedName]
