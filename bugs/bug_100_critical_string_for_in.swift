// BUG 100: [CRITICAL] String for-in iteration broken
//
// Two related issues with iterating over a String:
// 1. `for c in str` — loop variable 'c' is not bound (undefined name)
// 2. `for _ in str` — only iterates once regardless of string length
//
// EXPECTED: iterates over each character, count = 5
// ACTUAL: variable unbound, or only 1 iteration
//
// Build: tinyswift compile --no-prelude-import bugs/bug_100_critical_string_for_in.swift
// Run:   ./bug_100_critical_string_for_in

func main() -> Int {
  let s: String = "hello"

  // Issue 1: variable not bound
  // for c in s { print(c) }  // ERROR: use of undefined name 'c'

  // Issue 2: only iterates once
  var count: Int = 0
  for _ in s {
    count = count + 1
  }
  print(count)    // Prints 1, should print 5

  return 0
}
