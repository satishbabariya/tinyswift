// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef TINYSWIFT_COMMON_CONCEPTS_H_
#define TINYSWIFT_COMMON_CONCEPTS_H_

#include <concepts>

namespace TinySwift {

// True if `T` is the same as one of `OtherT`.
template <typename T, typename... OtherT>
concept SameAsOneOf = (std::same_as<T, OtherT> || ...);

}  // namespace TinySwift

#endif  // TINYSWIFT_COMMON_CONCEPTS_H_
