// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// INCLUDE-FILE: toolchain/testing/testdata/min_prelude/parts/bool.tinyswift
// INCLUDE-FILE: toolchain/testing/testdata/min_prelude/parts/copy.tinyswift
// INCLUDE-FILE: toolchain/testing/testdata/min_prelude/parts/destroy.tinyswift

// --- min_prelude/parts/optional.tinyswift

package Core library "prelude/parts/optional";

import library "prelude/parts/bool";
import library "prelude/parts/copy";
import library "prelude/parts/destroy";

class Optional(T:! Copy) {
  fn None() -> Self {
    returned var me: Self;
    me.has_value = false;
    return var;
  }

  fn Some(value: T) -> Self {
    return {.has_value = true, .value = value};
  }

  fn HasValue[self: Self]() -> bool { return self.has_value; }
  fn Get[self: Self]() -> T { return self.value; }

  var has_value: bool;
  var value: T;
}
