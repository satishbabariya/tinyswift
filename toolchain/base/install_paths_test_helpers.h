// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef TINYSWIFT_TOOLCHAIN_BASE_INSTALL_PATHS_TEST_HELPERS_H_
#define TINYSWIFT_TOOLCHAIN_BASE_INSTALL_PATHS_TEST_HELPERS_H_

#include "llvm/Support/VirtualFileSystem.h"
#include "toolchain/base/install_paths.h"

namespace TinySwift::Testing {

// Prepares the VFS with prelude files from the real filesystem.
auto AddPreludeFilesToVfs(
    const InstallPaths& install_paths,
    llvm::IntrusiveRefCntPtr<llvm::vfs::InMemoryFileSystem>& vfs) -> void;

}  // namespace TinySwift::Testing

#endif  // TINYSWIFT_TOOLCHAIN_BASE_INSTALL_PATHS_TEST_HELPERS_H_
