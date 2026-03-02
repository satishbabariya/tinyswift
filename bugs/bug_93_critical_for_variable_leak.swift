// BUG 93: [CRITICAL] for-in loop variable leaks into outer scope
//
// After a for-in loop, the loop variable's final value is visible in
// the outer scope instead of the previously-shadowed variable's value.
//
// EXPECTED: prints 1, 2, 3 then 99 (outer i restored)
// ACTUAL: prints 1, 2, 3 then 3 (loop variable leaked)
//
// Build: tinyswift compile --no-prelude-import bugs/bug_93_critical_for_variable_leak.swift
// Run:   ./bug_93_critical_for_variable_leak

func main() -> Int {
  let i: Int = 99
  for i in 1...3 {
    print(i)         // 1, 2, 3 (correct)
  }
  print(i)           // Prints 3 instead of 99
  return 0
}
