// BUG 101: [CRITICAL] switch fallthrough doesn't work
//
// The `fallthrough` keyword in switch cases is accepted syntactically
// but has no effect — execution does not continue to the next case.
//
// EXPECTED: prints "one" then "two"
// ACTUAL: prints "one" only
//
// Build: tinyswift compile --no-prelude-import bugs/bug_101_critical_switch_fallthrough.swift
// Run:   ./bug_101_critical_switch_fallthrough

func main() -> Int {
  let x: Int = 1
  switch x {
  case 1:
    print("one")
    fallthrough       // Should continue to case 2
  case 2:
    print("two")      // Never reached
  default:
    print("other")
  }
  return 0
}
