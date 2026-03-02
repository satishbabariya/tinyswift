// BUG 153: [SEMANTIC] Bool negation `!` prefix operator not parsed
//
// The `!` prefix operator for Bool negation is not recognized as a
// prefix operator. The parser reports "expected expression" when
// encountering `!variable`. Note: `!=` (not-equal) works correctly.
//
// EXPECTED: prints nothing (y is false, if body skipped)
// ACTUAL: error: expected expression (on !x)
//
// WORKAROUND: Use comparison: `if x == false { ... }` or
//   `if x != true { ... }` instead of `if !x { ... }`

func main() -> Int {
  let x: Bool = true
  let y: Bool = !x       // ERROR: expected expression
  if y { print("negated") }
  return 0
}
