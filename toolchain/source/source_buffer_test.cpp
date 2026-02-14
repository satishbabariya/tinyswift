// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "toolchain/source/source_buffer.h"

#include <gtest/gtest.h>

#include "common/check.h"
#include "llvm/Support/VirtualFileSystem.h"
#include "toolchain/diagnostics/emitter.h"

namespace TinySwift {
namespace {

static constexpr llvm::StringLiteral TestFileName = "test.swift";

TEST(SourceBufferTest, MissingFile) {
  llvm::vfs::InMemoryFileSystem fs;
  auto buffer = SourceBuffer::MakeFromFile(fs, TestFileName,
                                           Diagnostics::ConsoleConsumer());
  EXPECT_FALSE(buffer);
}

TEST(SourceBufferTest, SimpleFile) {
  llvm::vfs::InMemoryFileSystem fs;
  TINYSWIFT_CHECK(fs.addFile(TestFileName, /*ModificationTime=*/0,
                          llvm::MemoryBuffer::getMemBuffer("Hello World")));

  auto buffer = SourceBuffer::MakeFromFile(fs, TestFileName,
                                           Diagnostics::ConsoleConsumer());
  ASSERT_TRUE(buffer);

  EXPECT_EQ(TestFileName, buffer->filename());
  EXPECT_EQ("Hello World", buffer->text());
}

TEST(SourceBufferTest, NoNull) {
  llvm::vfs::InMemoryFileSystem fs;
  static constexpr char NoNull[3] = {'a', 'b', 'c'};
  TINYSWIFT_CHECK(fs.addFile(
      TestFileName, /*ModificationTime=*/0,
      llvm::MemoryBuffer::getMemBuffer(llvm::StringRef(NoNull, sizeof(NoNull)),
                                       /*BufferName=*/"",
                                       /*RequiresNullTerminator=*/false)));

  auto buffer = SourceBuffer::MakeFromFile(fs, TestFileName,
                                           Diagnostics::ConsoleConsumer());
  ASSERT_TRUE(buffer);

  EXPECT_EQ(TestFileName, buffer->filename());
  EXPECT_EQ("abc", buffer->text());
}

TEST(SourceBufferTest, EmptyFile) {
  llvm::vfs::InMemoryFileSystem fs;
  TINYSWIFT_CHECK(fs.addFile(TestFileName, /*ModificationTime=*/0,
                          llvm::MemoryBuffer::getMemBuffer("")));

  auto buffer = SourceBuffer::MakeFromFile(fs, TestFileName,
                                           Diagnostics::ConsoleConsumer());
  ASSERT_TRUE(buffer);

  EXPECT_EQ(TestFileName, buffer->filename());
  EXPECT_EQ("", buffer->text());
}

}  // namespace
}  // namespace TinySwift
