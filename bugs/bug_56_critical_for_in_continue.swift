// BUG 56: [CRITICAL] continue in for-in loop causes infinite loop
//
// The `continue` statement works correctly in while loops but causes an
// infinite loop (hangs forever) in for-in loops. The loop counter never
// advances past the continue point.
//
// EXPECTED: prints 1, 2, 4, 5 (skipping 3)
// ACTUAL: infinite loop / timeout
//
// While-loop continue works fine: only for-in is affected.
//
// Build: tinyswift compile --no-prelude-import bugs/bug_56_critical_for_in_continue.swift
// Run:   ./bug_56_critical_for_in_continue  (WARNING: will hang)

func main() -> Int {
  for i in 1...5 {
    if i == 3 { continue }  // Causes infinite loop
    print(i)
  }
  return 0
}

// While-loop equivalent works correctly:
// var i: Int = 0
// while i < 5 {
//   i = i + 1
//   if i == 3 { continue }
//   print(i)
// }
