// BUG 44: [CRITICAL] String interpolation with expressions produces empty
//
// String interpolation with expressions (like \(x + y)) produces an
// empty string for the expression part. Only simple variable references
// work in string interpolation.
//
// This is related to but distinct from Bug 39 (literals in interpolation).
// Bug 39: \(42) literal → empty
// Bug 44: \(x + y) expression → empty
// Working: \(x) simple variable → correct
//
// EXPECTED: "sum=15", "1+2=3"
// ACTUAL: "sum=", "1+2="
//
// WORKAROUND: Compute expression into a variable first, then interpolate.
//
// Build: tinyswift compile --no-prelude-import bugs/bug_44_critical_string_interp_expression.swift
// Run:   ./bug_44_critical_string_interp_expression

func main() -> Int {
  let x: Int = 5
  let y: Int = 10

  // Simple variable interpolation works:
  print("x=\(x)")     // Prints "x=5" (correct)

  // Expression interpolation is broken:
  print("sum=\(x + y)")  // Prints "sum=" (missing 15)

  // Multi-interpolation with expression at end:
  let a: Int = 1
  let b: Int = 2
  print("\(a)+\(b)=\(a+b)")  // Prints "1+2=" (missing 3)

  // Workaround: compute first
  let sum: Int = x + y
  print("sum=\(sum)")  // Prints "sum=15" (correct)

  return 0
}
