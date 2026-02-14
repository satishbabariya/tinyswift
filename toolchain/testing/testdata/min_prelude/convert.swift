// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// INCLUDE-FILE: toolchain/testing/testdata/min_prelude/parts/as.tinyswift
// INCLUDE-FILE: toolchain/testing/testdata/min_prelude/parts/copy.tinyswift
// EXTRA-ARGS: --custom-core --exclude-dump-file-prefix=min_prelude/

// --- min_prelude/convert.tinyswift

// A minimal prelude for testing conversion between types.
package Core library "prelude";

export import library "prelude/parts/as";
export import library "prelude/parts/copy";
