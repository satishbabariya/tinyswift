# Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
# Exceptions. See /LICENSE for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

load("//bazel/cc_rules:defs.bzl", "cc_library")

package(default_visibility = ["//visibility:public"])

filegroup(
    name = "testdata",
    srcs = glob(["testdata/**/*.swift"], allow_empty = True),
)

cc_library(
    name = "language_server",
    srcs = ["language_server.cpp"],
    hdrs = ["language_server.h"],
    deps = [
        "//common:ostream",
        "//toolchain/diagnostics:emitter",
        "@llvm-project//llvm:Support",
    ],
)
