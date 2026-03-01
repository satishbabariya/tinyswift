// BUG 103: [CRITICAL] Overflow operators (&+, &*) don't perform operation
//
// The wrapping overflow operators &+ and &* are parsed but don't actually
// compute — they appear to return the left operand unchanged.
//
// EXPECTED: prints -9223372036854775808 (wrapped overflow)
// ACTUAL: prints 9223372036854775807 (left operand returned unchanged)
//
// Build: tinyswift compile --no-prelude-import bugs/bug_103_critical_overflow_operators.swift
// Run:   ./bug_103_critical_overflow_operators

func main() -> Int {
  let max: Int = 9223372036854775807
  let wrapped: Int = max &+ 1
  print(wrapped)    // Prints 9223372036854775807 (unchanged!)

  let big: Int = 1000000000
  let mul: Int = big &* 1000000000
  print(mul)        // Prints 1000000000 (unchanged!)

  return 0
}
