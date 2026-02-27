// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// M98-M102: Coroutine-to-state-machine transform.
//
// This transform runs as Pass 3 after all function bodies are checked.
// It rewrites generator and async functions into state machines entirely
// in the SemIR layer, so NO changes are needed to SILGen, Lower, or CodeGen.
//
// Generator<T> is represented as a two-pointer struct:
//   { ptr frame_ptr, ptr resume_fn_ptr }
//
// The original generator function is rewritten to:
//   1. Allocate a frame (via __tinyswift_alloc) with {__state, params, locals}
//   2. Return Generator<T>{frame_ptr, &__funcname_resume}
//
// A synthetic resume function is generated:
//   func __funcname_resume(frame: ptr) -> Optional<T>
//   Implements a switch on __state, executing code between yield points.

#ifndef TINYSWIFT_TOOLCHAIN_CHECK_COROUTINE_TRANSFORM_H_
#define TINYSWIFT_TOOLCHAIN_CHECK_COROUTINE_TRANSFORM_H_

namespace TinySwift::Check {

class Context;

// Run coroutine transforms on all generator and async functions.
// Called as Pass 3 after Pass 2 (function bodies).
auto TransformCoroutines(Context& context) -> void;

}  // namespace TinySwift::Check

#endif  // TINYSWIFT_TOOLCHAIN_CHECK_COROUTINE_TRANSFORM_H_
