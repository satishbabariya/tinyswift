// BUG 12: [CRASH-RT] if-let segfaults
//
// `if let val = optional { ... }` compiles but segfaults at runtime.
//
// EXPECTED: prints 42
// ACTUAL: compiles but segfaults
//
// WORKAROUND: Use force unwrap (a!) or nil coalescing (a ?? default).
//
// Build: tinyswift compile --no-prelude-import bugs/bug_12_crash_rt_if_let.swift
// Run:   ./bug_12_crash_rt_if_let

func main() -> Int {
  let a: Int? = 42
  if let val = a {
    print(val)  // SEGFAULT
  }
  return 0
}
