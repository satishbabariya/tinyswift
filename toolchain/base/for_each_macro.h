// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef TINYSWIFT_TOOLCHAIN_BASE_FOR_EACH_MACRO_H_
#define TINYSWIFT_TOOLCHAIN_BASE_FOR_EACH_MACRO_H_

/// TINYSWIFT_FOR_EACH() will apply `macro` to each argument in the variadic
/// argument list, putting the output of `sep()` between each one.
///
/// The `sep` should be a function macro that returns a separator. Premade
/// separataors are provided as TINYSWIFT_FOR_EACH_XYZ() macros.
#define TINYSWIFT_FOR_EACH(macro, sep, ...)      \
  __VA_OPT__(TINYSWIFT_INTERNAL_FOR_EACH_EXPAND( \
      TINYSWIFT_INTERNAL_FOR_EACH(macro, sep, __VA_ARGS__)))

#define TINYSWIFT_FOR_EACH_COMMA() ,
#define TINYSWIFT_FOR_EACH_SEMI() ;
#define TINYSWIFT_FOR_EACH_CONCAT()

// Internal helpers

#define TINYSWIFT_INTERNAL_FOR_EACH(macro, sep, a1, ...)                 \
  macro(a1) __VA_OPT__(sep()) __VA_OPT__(                             \
      TINYSWIFT_INTERNAL_FOR_EACH_AGAIN TINYSWIFT_INTERNAL_FOR_EACH_PARENS( \
          macro, sep, __VA_ARGS__))
#define TINYSWIFT_INTERNAL_FOR_EACH_PARENS ()
#define TINYSWIFT_INTERNAL_FOR_EACH_AGAIN() TINYSWIFT_INTERNAL_FOR_EACH

#define TINYSWIFT_INTERNAL_FOR_EACH_EXPAND(...)                             \
  TINYSWIFT_INTERNAL_FOR_EACH_EXPAND1(                                      \
      TINYSWIFT_INTERNAL_FOR_EACH_EXPAND1(TINYSWIFT_INTERNAL_FOR_EACH_EXPAND1( \
          TINYSWIFT_INTERNAL_FOR_EACH_EXPAND1(__VA_ARGS__))))
#define TINYSWIFT_INTERNAL_FOR_EACH_EXPAND1(...)                            \
  TINYSWIFT_INTERNAL_FOR_EACH_EXPAND2(                                      \
      TINYSWIFT_INTERNAL_FOR_EACH_EXPAND2(TINYSWIFT_INTERNAL_FOR_EACH_EXPAND2( \
          TINYSWIFT_INTERNAL_FOR_EACH_EXPAND2(__VA_ARGS__))))
#define TINYSWIFT_INTERNAL_FOR_EACH_EXPAND2(...)                            \
  TINYSWIFT_INTERNAL_FOR_EACH_EXPAND3(                                      \
      TINYSWIFT_INTERNAL_FOR_EACH_EXPAND3(TINYSWIFT_INTERNAL_FOR_EACH_EXPAND3( \
          TINYSWIFT_INTERNAL_FOR_EACH_EXPAND3(__VA_ARGS__))))
#define TINYSWIFT_INTERNAL_FOR_EACH_EXPAND3(...) __VA_ARGS__

#endif  // TINYSWIFT_TOOLCHAIN_BASE_FOR_EACH_MACRO_H_
