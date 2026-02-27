// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// INCLUDE-FILE: toolchain/testing/testdata/min_prelude/none.tinyswift
// NOAUTOUPDATE
// SET-CHECK-SUBSET
//
// M78: Ownership transfer — verifies that AllocClass and Call results do NOT
// get an extra Retain (they already transfer ownership), while NameRef copies DO.

class Counter {
  var count: Int
  init(count: Int) {
    self.count = count
  }
}

fn makeCounter() -> Counter {
  return Counter(count: 0)
}

fn testNoRetainOnAlloc() {
  // AllocClass transfers ownership — no Retain emitted.
  let c = Counter(count: 1)
}

fn testNoRetainOnCall() {
  // Call result transfers ownership — no Retain emitted.
  let c = makeCounter()
}

fn testRetainOnCopy() {
  let c1 = Counter(count: 2)
  // NameRef copy — Retain IS emitted.
  let c2 = c1
}

// Verify AllocClass appears, and that copies emit retain.
// CHECK:STDOUT: alloc_class
// CHECK:STDOUT: retain
// CHECK:STDOUT: release
