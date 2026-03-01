# Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
# Exceptions. See /LICENSE for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

load("@rules_shell//shell:sh_test.bzl", "sh_test")
load("//bazel/cc_rules:defs.bzl", "cc_library", "cc_test")
load("//testing/file_test:rules.bzl", "file_test")

package(default_visibility = ["//visibility:public"])

filegroup(
    name = "all_testdata",
    srcs = glob(
        ["testdata/**/*.swift"],
        exclude = [
            "testdata/min_prelude/**",
            "testdata/driver/projects/**",
        ],
    ),
)

filegroup(
    name = "project_fixtures",
    srcs = glob(["testdata/driver/projects/**"]),
    visibility = ["//visibility:public"],
)

filegroup(
    name = "min_prelude",
    srcs = glob(["testdata/min_prelude/**/*.swift"]),
    visibility = ["//visibility:public"],
)

cc_library(
    name = "compile_helper",
    testonly = 1,
    srcs = ["compile_helper.cpp"],
    hdrs = ["compile_helper.h"],
    deps = [
        "//toolchain/diagnostics:emitter",
        "//toolchain/lex",
        "//toolchain/parse",
        "//toolchain/parse:tree",
        "//toolchain/source:source_buffer",
        "@llvm-project//llvm:Support",
    ],
)

cc_library(
    name = "coverage_helper",
    testonly = 1,
    hdrs = ["coverage_helper.h"],
    deps = [
        "//common:find",
        "//common:set",
        "@googletest//:gtest",
        "@llvm-project//llvm:Support",
        "@re2",
    ],
)

file_test(
    name = "file_test",
    size = "small",
    timeout = "moderate",  # Taking >60 seconds in GitHub actions
    srcs = ["file_test.cpp"],
    data = [
        ":min_prelude",
        "//toolchain/install:clang_headers",
        "//toolchain/install:clang_headers_manifest.txt",
    ],
    tests = [":all_testdata"],
    deps = [
        "//common:all_llvm_targets",
        "//common:error",
        "//testing/file_test:autoupdate",
        "//testing/file_test:file_test_base",
        "//toolchain/driver",
        "@abseil-cpp//absl/strings",
        "@llvm-project//llvm:Support",
    ],
)

cc_library(
    name = "yaml_test_helpers",
    testonly = 1,
    srcs = ["yaml_test_helpers.cpp"],
    hdrs = ["yaml_test_helpers.h"],
    deps = [
        "//common:error",
        "//common:ostream",
        "//common:raw_string_ostream",
        "@abseil-cpp//absl/strings",
        "@googletest//:gtest",
        "@llvm-project//llvm:Support",
    ],
)

cc_test(
    name = "yaml_test_helpers_test",
    size = "small",
    srcs = ["yaml_test_helpers_test.cpp"],
    deps = [
        ":yaml_test_helpers",
        "//common:error_test_helpers",
        "//testing/base:gtest_main",
        "@googletest//:gtest",
    ],
)

# Integration test exercising the CLI with real project fixtures.
# This builds and runs the tinyswift binary against example projects
# that use compile, build, run, and test subcommands.
sh_test(
    name = "cli_integration_test",
    size = "large",
    timeout = "moderate",
    srcs = ["testdata/driver/projects/cli_integration_test.sh"],
    data = [
        ":project_fixtures",
        "//examples:examples",
        "//toolchain:tinyswift",
    ],
    env = {
        "TINYSWIFT": "$(rootpath //toolchain:tinyswift)",
    },
)
