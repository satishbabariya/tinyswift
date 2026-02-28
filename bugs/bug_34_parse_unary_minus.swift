// BUG 34: [PARSE] Unary minus not supported
//
// `-42` as a literal is not parsed. The parser expects an expression
// after `-`.
//
// EXPECTED: compiles and prints -42
// ACTUAL: ERROR: expected expression
//
// WORKAROUND: Use `0 - 42` instead.
//
// Build: tinyswift compile --no-prelude-import bugs/bug_34_parse_unary_minus.swift

func main() -> Int {
  let x: Int = -42  // ERROR: expected expression
  print(x)
  return 0
}
