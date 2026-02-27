# Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
# Exceptions. See /LICENSE for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "tinyswift_runtime",
    srcs = ["tinyswift_runtime.c"],
    hdrs = ["tinyswift_runtime.h"],
    copts = [
        "-ffunction-sections",
        "-fdata-sections",
    ],
    linkopts = ["-lm"],
)
