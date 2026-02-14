# Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
# Exceptions. See /LICENSE for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

load("//bazel/cc_toolchains:defs.bzl", "cc_env")
load("run_tool.bzl", "run_tool")

# Support `bazel run` and create a convenience symlink for the tool in the
# toolchain's install.
run_tool(
    name = "tinyswift",
    data = ["//toolchain/install:install_data"],
    env = cc_env(),
    tool = "//toolchain/install:prefix/bin/tinyswift",
)

# A convenience target for running the toolchain with the full prelude
# available.
alias(
    name = "toolchain",
    actual = ":tinyswift",
)
