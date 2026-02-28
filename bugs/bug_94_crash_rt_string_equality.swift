// BUG 94: [CRASH-RT] String equality comparison (==, !=) segfaults
//
// Comparing two String values with == or != compiles but crashes
// with a segfault at runtime.
//
// This is distinct from Bug 24 (String comparison crash during codegen).
// Here the crash is at runtime, not during compilation.
//
// EXPECTED: prints "same"
// ACTUAL: segfault (signal 11)
//
// Build: tinyswift compile --no-prelude-import bugs/bug_94_crash_rt_string_equality.swift
// Run:   ./bug_94_crash_rt_string_equality

func main() -> Int {
  let a: String = "hello"
  let b: String = "hello"
  if a == b {              // RUNTIME SEGFAULT
    print("same")
  }
  return 0
}
