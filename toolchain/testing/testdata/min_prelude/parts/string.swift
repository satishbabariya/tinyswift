// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// INCLUDE-FILE: toolchain/testing/testdata/min_prelude/parts/uint.tinyswift

// --- min_prelude/parts/string.tinyswift

// A minimal prelude for testing using `Int` or `i32`; required for arrays.
package Core library "prelude/parts/string";

import library "prelude/parts/uint";

class String {
  private var ptr: u8*;
  private var size: u64;
}
