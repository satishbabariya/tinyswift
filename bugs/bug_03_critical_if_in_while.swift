// BUG 3: [CRITICAL] if inside while - loop body falls off
//
// When an if-body (without return) appears inside a while loop,
// the code after the if-body in the loop iteration never executes,
// and the loop doesn't continue.
//
// EXPECTED: 20 (0+2+4+6+8)
// ACTUAL: 0 (falls off after first matching iteration)
//
// WORKAROUND: Move if-body logic into a separate function that
// returns a value, or restructure using if with early return.
//
// Build: tinyswift compile --no-prelude-import bugs/bug_03_critical_if_in_while.swift
// Run:   ./bug_03_critical_if_in_while

func main() -> Int {
  var i: Int = 0
  var sum: Int = 0
  while i < 10 {
    if i % 2 == 0 {
      sum = sum + i   // Executes on first match, then falls off
    }
    i = i + 1         // NEVER REACHED after if-body executes
  }
  print(sum)           // Prints 0
  return 0
}
