// BUG 10: [CRASH-RT] else-if chains segfault
//
// `if ... else if ... else` chains always segfault at runtime,
// even when all branches return.
//
// EXPECTED: prints 3, 2, 1
// ACTUAL: compiles successfully but segfaults when run
//
// WORKAROUND: Use sequential if-return statements instead:
//   if x > 10 { return 3 }
//   if x > 5 { return 2 }
//   return 1
//
// Build: tinyswift compile --no-prelude-import bugs/bug_10_crash_rt_else_if_chains.swift
// Run:   ./bug_10_crash_rt_else_if_chains

func test(_ x: Int) -> Int {
  if x > 10 {
    return 3
  } else if x > 5 {
    return 2
  } else {
    return 1
  }
}

func main() -> Int {
  print(test(15))  // SEGFAULT
  print(test(7))
  print(test(2))
  return 0
}
