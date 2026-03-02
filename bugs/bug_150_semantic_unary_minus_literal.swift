// BUG 150: [SEMANTIC] Unary minus on numeric literals not parsed
//
// The `-` prefix operator is not recognized before numeric literals in
// variable initialization contexts. `-x` (minus a variable) works in
// expression contexts like `return -x`, but `-5` (minus a literal)
// causes "expected expression" errors.
//
// EXPECTED: prints -5
// ACTUAL: error: expected expression
//
// WORKAROUND: Use `0 - 5` or assign to variable first then negate.
//   let x: Int = 0 - 5

func main() -> Int {
  let x: Int = -5      // ERROR: expected expression
  print(x)
  return 0
}

// NOTE: These work:
//   func neg(_ x: Int) -> Int { return -x }  // OK
//   let y: Int = 0 - 5                        // OK
