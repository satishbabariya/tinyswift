// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// INCLUDE-FILE: toolchain/testing/testdata/min_prelude/parts/copy.tinyswift
// INCLUDE-FILE: toolchain/testing/testdata/min_prelude/parts/optional.tinyswift

// --- min_prelude/parts/iterate.tinyswift

package Core library "prelude/parts/iterate";

import library "prelude/parts/copy";
import library "prelude/parts/optional";

interface Iterate {
  let ElementType:! Copy;
  let CursorType:! type;
  fn NewCursor[self: Self]() -> CursorType;
  fn Next[self: Self](cursor: CursorType*) -> Optional(ElementType);
}
