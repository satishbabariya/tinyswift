# Part of the TinySwift Language project, under the Apache License v2.0 with LLVM
# Exceptions. See /LICENSE for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

load("//bazel:tinyswift_cc_toolchain_config.bzl", "tinyswift_cc_toolchain_suite")

package(default_visibility = ["//visibility:public"])

tinyswift_cc_toolchain_suite(
    name = "tinyswift_cc_toolchain",
    configs = [
        ("linux", "aarch64"),
        ("linux", "x86_64"),
        ("freebsd", "x86_64"),
        ("macos", "arm64"),
        ("macos", "x86_64"),
    ],
)
