// BUG 104: [PARSE] Discard assignment `_ = expr` not supported
//
// The wildcard pattern `_ = expression` to discard a function's return
// value is rejected by the parser with "expected expression".
//
// EXPECTED: compiles and prints "called"
// ACTUAL: error: expected expression
//
// WORKAROUND: Assign to a named variable instead: `let unused = expr`
//
// Build: tinyswift compile --no-prelude-import bugs/bug_104_parse_discard_assign.swift

func sideEffect() -> Int {
  print("called")
  return 42
}

func main() -> Int {
  _ = sideEffect()      // ERROR: expected expression
  return 0
}
