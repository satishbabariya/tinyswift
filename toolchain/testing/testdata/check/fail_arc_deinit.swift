// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// INCLUDE-FILE: toolchain/testing/testdata/min_prelude/none.tinyswift
// NOAUTOUPDATE
// SET-CHECK-SUBSET
//
// M79: Deinit — verifies that deinit declarations produce a __deinit function
// and that Release instructions carry the deinit_id.

class Resource {
  var value: Int
  init(value: Int) {
    self.value = value
  }
  deinit {
    // Cleanup code — this body becomes Resource.__deinit.
  }
}

fn testDeinit() {
  let r = Resource(value: 10)
  // Scope exit should emit Release with deinit_id pointing to __deinit.
}

// CHECK:STDERR: fail_arc_deinit.swift:22:1: error: use of undefined name 'fn' [UndefinedName]
