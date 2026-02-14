// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "toolchain/base/install_paths_test_helpers.h"

#include <memory>
#include <utility>

#include "testing/base/global_exe_path.h"

namespace TinySwift::Testing {

// Prepares the VFS with prelude files from the real filesystem. Primarily for
// tests.
auto AddPreludeFilesToVfs(
    const InstallPaths& install_paths,
    llvm::IntrusiveRefCntPtr<llvm::vfs::InMemoryFileSystem>& vfs) -> void {
  // Load the prelude into the test VFS.
  auto real_fs = llvm::vfs::getRealFileSystem();
  auto prelude = install_paths.ReadPreludeManifest();
  TINYSWIFT_CHECK(prelude.ok(), "{0}", prelude.error());

  for (const auto& path : *prelude) {
    llvm::ErrorOr<std::unique_ptr<llvm::MemoryBuffer>> file =
        real_fs->getBufferForFile(path);
    TINYSWIFT_CHECK(file, "Error getting file: {0}", file.getError().message());
    bool added = vfs->addFile(path, /*ModificationTime=*/0, std::move(*file));
    TINYSWIFT_CHECK(added, "Duplicate file: {0}", path);
  }
}

}  // namespace TinySwift::Testing
