// BUG 83: [CRITICAL] Multiple guard statements don't check condition correctly
//
// When a function contains multiple guard statements in sequence, the
// guard conditions are not evaluated correctly. The else branch is never
// taken even when the condition is false.
//
// Single guard works. Only sequences of guards fail.
//
// EXPECTED: test(0,1) returns "x bad"
// ACTUAL: test(0,1) returns "ok" (guard condition ignored)
//
// Build: tinyswift compile --no-prelude-import bugs/bug_83_critical_multi_guard.swift
// Run:   ./bug_83_critical_multi_guard

func test(_ x: Int, _ y: Int) -> String {
  guard x > 0 else { return "x bad" }
  guard y > 0 else { return "y bad" }
  return "ok"
}

func main() -> Int {
  print(test(1, 1))   // "ok" (correct)
  print(test(0, 1))   // "ok" (WRONG: should be "x bad")
  print(test(1, 0))   // "ok" (WRONG: should be "y bad")
  return 0
}
