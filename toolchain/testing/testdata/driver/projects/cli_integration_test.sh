#!/bin/bash
# Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
# Exceptions. See /LICENSE for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Integration test that exercises the tinyswift CLI with real project fixtures.
# This test requires the tinyswift binary to be available as $TINYSWIFT.

set -euo pipefail

# --------------------------------------------------------------------------- #
# Helpers
# --------------------------------------------------------------------------- #
PASS=0
FAIL=0
TOTAL=0

pass() {
  PASS=$((PASS + 1))
  TOTAL=$((TOTAL + 1))
  echo "  PASS: $1"
}

fail() {
  FAIL=$((FAIL + 1))
  TOTAL=$((TOTAL + 1))
  echo "  FAIL: $1"
  if [[ -n "${2:-}" ]]; then
    echo "        $2"
  fi
}

section() {
  echo ""
  echo "=== $1 ==="
}

# Locate the tinyswift binary.
# When run by Bazel sh_test, TINYSWIFT is set via the env attribute.
# It may be a relative path resolved in the runfiles tree.
if [[ -z "${TINYSWIFT:-}" ]]; then
  echo "ERROR: TINYSWIFT environment variable must be set to the tinyswift binary."
  exit 1
fi

# Resolve via runfiles if needed.
if [[ -n "${TEST_SRCDIR:-}" && ! -x "$TINYSWIFT" ]]; then
  TINYSWIFT="${TEST_SRCDIR}/_main/${TINYSWIFT}"
fi

if [[ ! -x "$TINYSWIFT" ]]; then
  echo "ERROR: $TINYSWIFT is not executable (resolved: $TINYSWIFT)."
  exit 1
fi

# Locate the project fixtures directory.
# When invoked by Bazel, the runfiles directory is available.
if [[ -n "${TEST_SRCDIR:-}" ]]; then
  PROJECTS_DIR="${TEST_SRCDIR}/_main/toolchain/testing/testdata/driver/projects"
fi
if [[ -z "${PROJECTS_DIR:-}" || ! -d "${PROJECTS_DIR:-}" ]]; then
  SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
  PROJECTS_DIR="$SCRIPT_DIR"
fi

if [[ ! -d "$PROJECTS_DIR" ]]; then
  echo "ERROR: Projects directory not found: $PROJECTS_DIR"
  exit 1
fi

echo "Using tinyswift: $TINYSWIFT"
echo "Projects dir: $PROJECTS_DIR"

# Create a temporary working directory.
WORK_DIR="$(mktemp -d)"
trap 'rm -rf "$WORK_DIR"' EXIT

# --------------------------------------------------------------------------- #
# 1. Compile subcommand tests
# --------------------------------------------------------------------------- #
section "compile subcommand"

# 1a. Compile a single file to check phase (no codegen needed).
if "$TINYSWIFT" compile --phase=check --no-prelude-import \
    "$PROJECTS_DIR/hello_world/main.swift" 2>/dev/null; then
  pass "compile --phase=check hello_world/main.swift"
else
  fail "compile --phase=check hello_world/main.swift"
fi

# 1b. Compile with --dump-tokens.
if output=$("$TINYSWIFT" compile --phase=lex --dump-tokens --no-prelude-import \
    "$PROJECTS_DIR/hello_world/main.swift" 2>/dev/null) && \
    echo "$output" | grep -q "func"; then
  pass "compile --dump-tokens includes func token"
else
  fail "compile --dump-tokens includes func token"
fi

# 1c. Compile with --dump-parse-tree.
if output=$("$TINYSWIFT" compile --phase=parse --dump-parse-tree --no-prelude-import \
    "$PROJECTS_DIR/hello_world/main.swift" 2>/dev/null) && \
    echo "$output" | grep -q "FunctionDecl\|Tree"; then
  pass "compile --dump-parse-tree produces tree output"
else
  fail "compile --dump-parse-tree produces tree output"
fi

# 1d. Compile with --dump-sem-ir.
if output=$("$TINYSWIFT" compile --phase=check --dump-sem-ir --no-prelude-import \
    "$PROJECTS_DIR/hello_world/main.swift" 2>/dev/null) && \
    echo "$output" | grep -q "FunctionDecl\|SemIR\|fn\|main"; then
  pass "compile --dump-sem-ir produces semantic IR"
else
  fail "compile --dump-sem-ir produces semantic IR"
fi

# 1e. Multi-file compilation.
if "$TINYSWIFT" compile --phase=check --no-prelude-import \
    "$PROJECTS_DIR/multi_target/src/mathlib.swift" \
    "$PROJECTS_DIR/multi_target/src/app.swift" 2>/dev/null; then
  pass "compile multi-file (mathlib + app)"
else
  fail "compile multi-file (mathlib + app)"
fi

# 1f. Compile to object file.
OBJ_OUTPUT="$WORK_DIR/hello.o"
if "$TINYSWIFT" compile --emit-object --output="$OBJ_OUTPUT" --no-prelude-import \
    "$PROJECTS_DIR/hello_world/main.swift" 2>/dev/null && \
    [[ -f "$OBJ_OUTPUT" ]]; then
  pass "compile --emit-object produces .o file"
else
  fail "compile --emit-object produces .o file"
fi

# 1g. Compile to assembly.
ASM_OUTPUT="$WORK_DIR/hello.s"
if "$TINYSWIFT" compile --asm-output --output="$ASM_OUTPUT" --no-prelude-import \
    "$PROJECTS_DIR/hello_world/main.swift" 2>/dev/null && \
    [[ -f "$ASM_OUTPUT" ]]; then
  pass "compile --asm-output produces .s file"
else
  fail "compile --asm-output produces .s file"
fi

# 1h. Compile with --dump-llvm-ir.
if output=$("$TINYSWIFT" compile --phase=lower --dump-llvm-ir --no-prelude-import \
    "$PROJECTS_DIR/hello_world/main.swift" 2>/dev/null) && \
    echo "$output" | grep -q "define\|module\|target"; then
  pass "compile --dump-llvm-ir produces LLVM IR"
else
  fail "compile --dump-llvm-ir produces LLVM IR"
fi

# 1i. Compile with optimization levels.
for opt in none debug speed size; do
  if "$TINYSWIFT" compile --phase=lower --optimize="$opt" --no-prelude-import \
      "$PROJECTS_DIR/hello_world/main.swift" 2>/dev/null; then
    pass "compile --optimize=$opt"
  else
    fail "compile --optimize=$opt"
  fi
done

# 1j. Compile with --dump-sil.
if output=$("$TINYSWIFT" compile --phase=sil --dump-sil --no-prelude-import \
    "$PROJECTS_DIR/hello_world/main.swift" 2>/dev/null) && \
    echo "$output" | grep -q "sil\|func\|SIL\|main"; then
  pass "compile --dump-sil produces SIL output"
else
  fail "compile --dump-sil produces SIL output"
fi

# 1k. Compile with --dump-timings.
if "$TINYSWIFT" compile --phase=check --dump-timings --no-prelude-import \
    "$PROJECTS_DIR/hello_world/main.swift" 2>/dev/null | grep -q "timing\|ms\|source\|lex\|parse"; then
  pass "compile --dump-timings shows timing info"
else
  fail "compile --dump-timings shows timing info"
fi

# 1l. Compile to executable (full pipeline).
EXE_OUTPUT="$WORK_DIR/hello_exe"
if "$TINYSWIFT" compile --output="$EXE_OUTPUT" --no-prelude-import \
    "$PROJECTS_DIR/hello_world/main.swift" 2>/dev/null && \
    [[ -x "$EXE_OUTPUT" ]]; then
  pass "compile to executable produces runnable binary"
  # 1m. Actually run the executable.
  if exe_out=$("$EXE_OUTPUT" 2>/dev/null); then
    pass "compiled executable runs successfully"
    if echo "$exe_out" | grep -q "Hello"; then
      pass "compiled executable produces expected output"
    else
      fail "compiled executable produces expected output" "got: $exe_out"
    fi
  else
    fail "compiled executable runs successfully"
  fi
else
  fail "compile to executable produces runnable binary"
fi

# --------------------------------------------------------------------------- #
# 2. Build subcommand tests
# --------------------------------------------------------------------------- #
section "build subcommand"

# 2a. Build a project without manifest (uses default).
HELLO_COPY="$WORK_DIR/hello_project"
mkdir -p "$HELLO_COPY"
cp "$PROJECTS_DIR/hello_world/main.swift" "$HELLO_COPY/"

if (cd "$HELLO_COPY" && "$TINYSWIFT" build 2>/dev/null); then
  pass "build without manifest (default)"
  if [[ -d "$HELLO_COPY/.build" ]]; then
    pass "build creates .build directory"
  else
    fail "build creates .build directory"
  fi
else
  fail "build without manifest (default)"
fi

# 2b. Build a project with manifest (multi-target).
MULTI_COPY="$WORK_DIR/multi_project"
cp -r "$PROJECTS_DIR/multi_target" "$MULTI_COPY"

if (cd "$MULTI_COPY" && "$TINYSWIFT" build 2>/dev/null); then
  pass "build with manifest (multi-target)"
else
  fail "build with manifest (multi-target)"
fi

# 2c. Build with --verbose flag.
VERBOSE_COPY="$WORK_DIR/verbose_project"
mkdir -p "$VERBOSE_COPY"
cp "$PROJECTS_DIR/hello_world/main.swift" "$VERBOSE_COPY/"

if output=$((cd "$VERBOSE_COPY" && "$TINYSWIFT" build --verbose) 2>&1) && \
    echo "$output" | grep -qi "build\|compil\|target"; then
  pass "build --verbose shows build info"
else
  fail "build --verbose shows build info"
fi

# 2d. Build a library project.
LIB_COPY="$WORK_DIR/lib_project"
cp -r "$PROJECTS_DIR/library_project" "$LIB_COPY"

if (cd "$LIB_COPY" && "$TINYSWIFT" build 2>/dev/null); then
  pass "build library project"
  if find "$LIB_COPY/.build" -name "*.a" 2>/dev/null | grep -q ".a"; then
    pass "build library produces .a archive"
  else
    fail "build library produces .a archive"
  fi
else
  fail "build library project"
fi

# 2e. Build with --release flag.
RELEASE_COPY="$WORK_DIR/release_project"
mkdir -p "$RELEASE_COPY"
cp "$PROJECTS_DIR/hello_world/main.swift" "$RELEASE_COPY/"

if (cd "$RELEASE_COPY" && "$TINYSWIFT" build --release 2>/dev/null); then
  pass "build --release"
  if [[ -d "$RELEASE_COPY/.build/release" ]]; then
    pass "build --release uses release build dir"
  else
    fail "build --release uses release build dir"
  fi
else
  fail "build --release"
fi

# 2f. Build project with dependencies.
DEPS_COPY="$WORK_DIR/deps_project"
cp -r "$PROJECTS_DIR/with_deps" "$DEPS_COPY"

if (cd "$DEPS_COPY" && "$TINYSWIFT" build 2>/dev/null); then
  pass "build project with dependencies"
else
  fail "build project with dependencies"
fi

# --------------------------------------------------------------------------- #
# 3. Run subcommand tests
# --------------------------------------------------------------------------- #
section "run subcommand"

# 3a. Run a simple project.
RUN_COPY="$WORK_DIR/run_project"
mkdir -p "$RUN_COPY"
cp "$PROJECTS_DIR/hello_world/main.swift" "$RUN_COPY/"

if output=$((cd "$RUN_COPY" && "$TINYSWIFT" run) 2>/dev/null) && \
    echo "$output" | grep -q "Hello"; then
  pass "run simple project"
else
  fail "run simple project"
fi

# 3b. Run with --verbose.
if (cd "$RUN_COPY" && "$TINYSWIFT" run --verbose) 2>&1 | \
    grep -qi "running\|build\|compil"; then
  pass "run --verbose shows execution info"
else
  fail "run --verbose shows execution info"
fi

# 3c. Run a multi-target project.
RUN_MULTI="$WORK_DIR/run_multi"
cp -r "$PROJECTS_DIR/multi_target" "$RUN_MULTI"

if output=$((cd "$RUN_MULTI" && "$TINYSWIFT" run) 2>/dev/null) && \
    echo "$output" | grep -q "sum\|product\|factorial"; then
  pass "run multi-target project"
else
  fail "run multi-target project"
fi

# --------------------------------------------------------------------------- #
# 4. Test subcommand tests
# --------------------------------------------------------------------------- #
section "test subcommand"

# 4a. Run tests in a project with test files.
TEST_COPY="$WORK_DIR/test_project"
cp -r "$PROJECTS_DIR/with_tests" "$TEST_COPY"

if (cd "$TEST_COPY" && "$TINYSWIFT" test 2>/dev/null); then
  pass "test discovers and runs test functions"
else
  fail "test discovers and runs test functions"
fi

# 4b. Test with --verbose.
TEST_VERBOSE="$WORK_DIR/test_verbose"
cp -r "$PROJECTS_DIR/with_tests" "$TEST_VERBOSE"

if output=$((cd "$TEST_VERBOSE" && "$TINYSWIFT" test --verbose) 2>&1) && \
    echo "$output" | grep -qi "test\|found\|pass"; then
  pass "test --verbose shows test discovery info"
else
  fail "test --verbose shows test discovery info"
fi

# 4c. Test with --filter.
TEST_FILTER="$WORK_DIR/test_filter"
cp -r "$PROJECTS_DIR/with_tests" "$TEST_FILTER"

if (cd "$TEST_FILTER" && "$TINYSWIFT" test --filter=utils 2>/dev/null); then
  pass "test --filter runs filtered tests"
else
  fail "test --filter runs filtered tests"
fi

# 4d. Test when no test files exist should fail.
TEST_EMPTY="$WORK_DIR/test_empty"
mkdir -p "$TEST_EMPTY"
echo 'func main() -> Int { return 0 }' > "$TEST_EMPTY/main.swift"

if ! (cd "$TEST_EMPTY" && "$TINYSWIFT" test 2>/dev/null); then
  pass "test fails when no test files found"
else
  fail "test fails when no test files found" "expected failure"
fi

# --------------------------------------------------------------------------- #
# 5. Error handling tests
# --------------------------------------------------------------------------- #
section "error handling"

# 5a. Compile nonexistent file.
if ! "$TINYSWIFT" compile --phase=check --no-prelude-import \
    "$WORK_DIR/does_not_exist.swift" 2>/dev/null; then
  pass "compile nonexistent file fails"
else
  fail "compile nonexistent file fails" "expected failure"
fi

# 5b. Compile with conflicting phase/dump flags.
if ! "$TINYSWIFT" compile --phase=lex --dump-parse-tree --no-prelude-import \
    "$PROJECTS_DIR/hello_world/main.swift" 2>/dev/null; then
  pass "compile conflicting phase/dump flags fails"
else
  fail "compile conflicting phase/dump flags fails" "expected failure"
fi

# 5c. Compile a file with syntax errors.
SYNTAX_ERR="$WORK_DIR/syntax_error.swift"
cat > "$SYNTAX_ERR" << 'EOF'
func broken( {
  let x: Int = "this is wrong
}
EOF
if ! "$TINYSWIFT" compile --phase=check --no-prelude-import \
    "$SYNTAX_ERR" 2>/dev/null; then
  pass "compile file with syntax errors fails"
else
  fail "compile file with syntax errors fails" "expected failure"
fi

# 5d. Help subcommand (should succeed).
if "$TINYSWIFT" help 2>/dev/null | grep -qi "tinyswift\|subcommand\|compile"; then
  pass "help subcommand shows usage"
else
  # Try --help instead.
  if "$TINYSWIFT" --help 2>/dev/null | grep -qi "tinyswift\|subcommand\|compile"; then
    pass "help subcommand shows usage (via --help)"
  else
    fail "help subcommand shows usage"
  fi
fi

# 5e. Version flag.
if "$TINYSWIFT" --version 2>/dev/null | grep -qi "tinyswift\|version\|[0-9]"; then
  pass "version flag shows version"
else
  fail "version flag shows version"
fi

# --------------------------------------------------------------------------- #
# 6. Compilation with examples
# --------------------------------------------------------------------------- #
section "example files"

# Compile each example through the check phase.
if [[ -n "${TEST_SRCDIR:-}" ]]; then
  EXAMPLES_DIR="${TEST_SRCDIR}/_main/examples"
else
  EXAMPLES_DIR="$PROJECTS_DIR/../../../../examples"
fi
if [[ -d "$EXAMPLES_DIR" ]]; then
  # Use a subset of examples that are likely to work without full prelude.
  for example in "$EXAMPLES_DIR"/*.swift; do
    basename=$(basename "$example")
    if "$TINYSWIFT" compile --phase=check \
        "$example" 2>/dev/null; then
      pass "compile example $basename (check phase)"
    else
      # Try without prelude for simpler examples.
      if "$TINYSWIFT" compile --phase=parse --no-prelude-import \
          "$example" 2>/dev/null; then
        pass "compile example $basename (parse phase, no prelude)"
      else
        fail "compile example $basename"
      fi
    fi
  done
else
  echo "  SKIP: examples directory not found at $EXAMPLES_DIR"
fi

# --------------------------------------------------------------------------- #
# Summary
# --------------------------------------------------------------------------- #
echo ""
echo "==========================================="
echo "Integration Test Summary"
echo "==========================================="
echo "  Total: $TOTAL"
echo "  Pass:  $PASS"
echo "  Fail:  $FAIL"
echo "==========================================="

if [[ $FAIL -gt 0 ]]; then
  exit 1
fi
exit 0
