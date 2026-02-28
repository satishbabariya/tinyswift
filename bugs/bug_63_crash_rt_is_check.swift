// BUG 63: [CRASH-RT] `is` type check operator crashes at runtime
//
// Using the `is` operator to check a value's type compiles successfully
// but crashes with a segfault at runtime.
//
// EXPECTED: prints "yes"
// ACTUAL: segfault (signal 11)
//
// Build: tinyswift compile --no-prelude-import bugs/bug_63_crash_rt_is_check.swift
// Run:   ./bug_63_crash_rt_is_check

func main() -> Int {
  let x: Int = 42
  if x is Int {       // RUNTIME SEGFAULT
    print("yes")
  }
  return 0
}
