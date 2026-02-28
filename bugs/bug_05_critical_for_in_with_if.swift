// BUG 5: [CRITICAL] for-in with if - same as Bug 3
//
// Same if-body fall-off bug in for-in loops.
//
// EXPECTED: 30 (2+4+6+8+10)
// ACTUAL: 0 (falls off after first matching iteration)
//
// Build: tinyswift compile --no-prelude-import bugs/bug_05_critical_for_in_with_if.swift
// Run:   ./bug_05_critical_for_in_with_if

func main() -> Int {
  var sum: Int = 0
  for i in 1...10 {
    if i % 2 == 0 {
      sum = sum + i
    }
  }
  print(sum)  // Prints 0 instead of 30
  return 0
}
