// BUG 9: [CRITICAL] Switch with multiple case values doesn't match
//
// `case 1, 5, 9:` compiles but none of the values match.
//
// EXPECTED: test(1) returns true, test(5) returns true
// ACTUAL: test(1) returns false, test(5) returns false
//
// Build: tinyswift compile --no-prelude-import bugs/bug_09_critical_switch_multi_value_case.swift
// Run:   ./bug_09_critical_switch_multi_value_case

func test(_ c: Int) -> Bool {
  switch c {
  case 1, 5, 9: return true
  default: return false
  }
}

func main() -> Int {
  print(test(1))   // Should print true, prints false
  print(test(5))   // Should print true, prints false
  print(test(9))   // Should print true, prints false
  print(test(2))   // Should print false (correct)
  return 0
}
