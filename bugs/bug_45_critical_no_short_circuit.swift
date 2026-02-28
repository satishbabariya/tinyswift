// BUG 45: [CRITICAL] No short-circuit evaluation for && and || operators
//
// The && and || operators evaluate BOTH operands regardless of the
// left-hand value. This means:
//   - `false && expr` still evaluates expr
//   - `true || expr` still evaluates expr
//
// This can cause unexpected side effects and incorrect behavior when
// the right-hand operand has side effects or depends on a guard.
//
// EXPECTED: no output (short-circuit should skip right-hand side)
// ACTUAL: "right side called" printed (right-hand evaluated eagerly)
//
// Build: tinyswift compile --no-prelude-import bugs/bug_45_critical_no_short_circuit.swift
// Run:   ./bug_45_critical_no_short_circuit

func sideEffect() -> Bool {
  print("right side called")
  return true
}

func main() -> Int {
  // && should short-circuit: false && anything = false
  // sideEffect() should NOT be called
  let a: Bool = false && sideEffect()
  // ACTUAL: prints "right side called"

  // || should short-circuit: true || anything = true
  // sideEffect() should NOT be called
  let b: Bool = true || sideEffect()
  // ACTUAL: prints "right side called" again

  return 0
}
