// BUG 111: [CRITICAL] Guard else block never executes
//
// When a guard condition evaluates to false, the else block is completely
// skipped. Execution falls through to the code after the guard statement
// as if the guard were not there.
//
// Even `guard false else { return 42 }` falls through and never returns 42.
// Guard when condition is TRUE works correctly (falls through as expected).
//
// This supersedes Bug 83 (multiple guards) - the root cause is that
// guard-else blocks never execute, regardless of single or multiple guards.
//
// EXPECTED: prints "0" (guard fails, else returns 0)
// ACTUAL: prints "-10" (guard skipped, falls through to x * 10)
//
// WORKAROUND: Use if-return instead of guard:
//   if x <= 0 { return 0 }   // instead of guard x > 0 else { return 0 }
//
// Build: tinyswift compile --no-prelude-import --output=OUT bugs/bug_111_critical_guard_else_never_executes.swift

func validate(_ x: Int) -> Int {
  guard x > 0 else { return 0 }   // else block NEVER runs
  return x * 10
}

func main() -> Int {
  print(validate(5))      // Prints 50 (correct - guard passes)
  print(validate(0 - 1))  // Prints -10 (WRONG - should print 0)
  return 0
}
