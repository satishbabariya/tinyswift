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
    name = "options",
    hdrs = ["options.h"],
    deps = [
        "@llvm-project//llvm:Support",
    ],
)

cc_library(
    name = "lower",
    srcs = ["lower.cpp"],
    hdrs = ["lower.h"],
    deps = [
        ":options",
        "//common:check",
        "//common:vlog",
        "//toolchain/sem_ir:file",
        "@llvm-project//llvm:Core",
        "@llvm-project//llvm:Support",
    ],
)
