// BUG 42: [CRITICAL] Nested ternary operator evaluates wrong branch
//
// When a ternary operator appears in the false branch of another ternary,
// the result is always the true-branch value of the OUTER ternary, even
// when the outer condition is false.
//
// Simple (non-nested) ternaries work correctly.
// Even explicit parentheses don't fix the issue.
//
// EXPECTED: prints 2 (x=5: x>10 is false, x>3 is true → 2)
// ACTUAL: prints 1 (always returns outer true-branch)
//
// WORKAROUND: Use sequential if-return statements instead.
//
// Build: tinyswift compile --no-prelude-import bugs/bug_42_critical_nested_ternary.swift
// Run:   ./bug_42_critical_nested_ternary

func main() -> Int {
  let x: Int = 5

  // Simple ternaries work:
  let a: Int = x > 10 ? 100 : 200  // Correctly returns 200
  print(a)

  let b: Int = x > 3 ? 300 : 400   // Correctly returns 300
  print(b)

  // Nested ternary is broken:
  let c: Int = x > 10 ? 1 : x > 3 ? 2 : 3
  print(c)  // Prints 1, should print 2

  // Even with explicit parentheses:
  let d: Int = (x > 10) ? 1 : ((x > 3) ? 2 : 3)
  print(d)  // Prints 1, should print 2

  return 0
}
