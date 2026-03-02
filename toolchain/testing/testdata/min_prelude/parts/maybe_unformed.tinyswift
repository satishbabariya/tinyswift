// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// INCLUDE-FILE: toolchain/testing/testdata/min_prelude/parts/destroy.tinyswift

// --- min_prelude/parts/maybe_unformed.tinyswift

package Core library "prelude/parts/maybe_unformed";

export import library "prelude/parts/destroy";

private fn Make(t: type) -> type = "maybe_unformed.make_type";

class MaybeUnformed(T:! type) {
  adapt Make(T);
}
