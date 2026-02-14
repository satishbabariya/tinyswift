// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "common/raw_hashtable.h"

#include <cstddef>

namespace TinySwift::RawHashtable {

volatile std::byte global_addr_seed{1};

}  // namespace TinySwift::RawHashtable
