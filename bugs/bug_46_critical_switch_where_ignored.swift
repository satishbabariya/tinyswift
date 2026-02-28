// BUG 46: [CRITICAL] switch where clause condition is ignored
//
// In a switch statement, `case let n where <condition>:` always matches
// regardless of whether the where condition is true. The where guard
// is completely ignored, and every value matches the first where-case.
//
// EXPECTED: classify(200)=3, classify(50)=2, classify(5)=1, classify(0)=0
// ACTUAL: classify(200)=3, classify(50)=3, classify(5)=3, classify(0)=3
//
// Build: tinyswift compile --no-prelude-import bugs/bug_46_critical_switch_where_ignored.swift
// Run:   ./bug_46_critical_switch_where_ignored

func classify(_ x: Int) -> Int {
  switch x {
  case let n where n > 100: return 3
  case let n where n > 10: return 2
  case let n where n > 0: return 1
  default: return 0
  }
}

func main() -> Int {
  print(classify(200))  // 3 (correct by accident)
  print(classify(50))   // Should print 2, prints 3
  print(classify(5))    // Should print 1, prints 3
  print(classify(0))    // Should print 0, prints 3
  return 0
}
