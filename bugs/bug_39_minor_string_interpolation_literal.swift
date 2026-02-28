// BUG 39: [MINOR] String interpolation with literals is empty
//
// `\(42)` (literal) in string interpolation produces nothing,
// but `\(variable)` works correctly.
//
// EXPECTED: prints "value: 42"
// ACTUAL: prints "value: " (missing the literal value)
//
// WORKAROUND: Assign to a variable first, then interpolate.
//
// Build: tinyswift compile --no-prelude-import bugs/bug_39_minor_string_interpolation_literal.swift
// Run:   ./bug_39_minor_string_interpolation_literal

func main() -> Int {
  print("value: \(42)")  // Prints "value: " (missing 42)
  let x: Int = 42
  print("value: \(x)")   // Prints "value: 42" (works)
  return 0
}
