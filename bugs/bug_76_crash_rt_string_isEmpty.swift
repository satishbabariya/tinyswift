// BUG 76: [CRASH-RT] String.isEmpty property crashes at runtime
//
// Accessing the .isEmpty property on a String compiles but crashes
// with a segfault at runtime.
//
// EXPECTED: prints "empty"
// ACTUAL: segfault (signal 11)
//
// Build: tinyswift compile --no-prelude-import bugs/bug_76_crash_rt_string_isEmpty.swift
// Run:   ./bug_76_crash_rt_string_isEmpty

func main() -> Int {
  let a: String = ""
  if a.isEmpty {           // RUNTIME SEGFAULT
    print("empty")
  }
  return 0
}
