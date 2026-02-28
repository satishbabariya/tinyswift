// BUG 36: [PARSE] Implicit member expressions not supported
//
// `.memberName` shorthand for enum cases is not parsed as an
// expression (only works in switch case patterns).
//
// EXPECTED: compiles and runs
// ACTUAL: Parser crash
//
// WORKAROUND: Use full type name: `Result.success(value)`
//
// Build: tinyswift compile --no-prelude-import bugs/bug_36_parse_implicit_member_expression.swift

enum Color {
  case red
  case green
  case blue
}

func test() -> Color {
  return .red  // Parser crash - implicit member expression
}

func main() -> Int {
  print(0)
  return 0
}
