// BUG 8: [CRITICAL] Switch range patterns never match
//
// `case 1...5:` in a switch compiles but never matches any value.
// Always falls through to default.
//
// EXPECTED: test(3) returns 1, test(7) returns 2
// ACTUAL: test(3) returns 0, test(7) returns 0
//
// Build: tinyswift compile --no-prelude-import bugs/bug_08_critical_switch_range_pattern.swift
// Run:   ./bug_08_critical_switch_range_pattern

func test(_ x: Int) -> Int {
  switch x {
  case 1...5: return 1
  case 6...10: return 2
  default: return 0
  }
}

func main() -> Int {
  print(test(3))   // Should print 1, prints 0
  print(test(7))   // Should print 2, prints 0
  print(test(0))   // Should print 0, prints 0 (correct by accident)
  return 0
}
