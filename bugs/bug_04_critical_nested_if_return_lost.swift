// BUG 4: [CRITICAL] Nested if with return - inner return lost
//
// When an inner if-body has a return statement inside an outer
// if-body, the return is lost and execution falls through.
//
// EXPECTED: test(200) returns 999
// ACTUAL: test(200) returns 10 (inner return lost)
//
// Build: tinyswift compile --no-prelude-import bugs/bug_04_critical_nested_if_return_lost.swift
// Run:   ./bug_04_critical_nested_if_return_lost

func test(_ x: Int) -> Int {
  if x > 0 {
    if x > 100 {
      return 999    // LOST - never returns this
    }
    return 10       // Falls here for ALL x > 0
  }
  return 0
}

func main() -> Int {
  print(test(200))  // Should print 999, prints 10
  print(test(50))   // Should print 10, prints 10 (correct by accident)
  print(test(0))    // Should print 0 (works because outer if not entered)
  return 0
}
