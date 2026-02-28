// BUG 50: [PARSE] Semicolons as statement separators fail in all code blocks
//
// Semicolons between statements crash the parser in function bodies,
// if-bodies, while-bodies, and all other code blocks. This extends
// Bug 37 which documented the issue only for struct bodies.
//
// EXPECTED: compiles and prints 42
// ACTUAL: error: expected expression (at the semicolon)
//
// WORKAROUND: Use newlines to separate statements.
//
// Build: tinyswift compile --no-prelude-import bugs/bug_50_parse_semicolons_everywhere.swift

func main() -> Int {
  // Semicolons in function body:
  let x: Int = 5; print(x)  // ERROR: expected expression

  // Semicolons in if-body:
  // if true { let a: Int = 1; print(a) }  // Also fails

  return 0
}
