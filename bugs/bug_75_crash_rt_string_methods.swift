// BUG 75: [CRASH-RT] String.contains/hasPrefix/hasSuffix crash at runtime
//
// String methods that take a String parameter (.contains(), .hasPrefix(),
// .hasSuffix()) compile but crash with a segfault at runtime.
//
// EXPECTED: prints "yes"
// ACTUAL: segfault (signal 11)
//
// Build: tinyswift compile --no-prelude-import bugs/bug_75_crash_rt_string_methods.swift
// Run:   ./bug_75_crash_rt_string_methods

func main() -> Int {
  let s: String = "hello world"

  // All of these crash at runtime:
  if s.contains("world") {     // RUNTIME SEGFAULT
    print("yes")
  }

  // Also crashes:
  // if s.hasPrefix("hello") { print("yes") }
  // if s.hasSuffix("world") { print("yes") }

  return 0
}
