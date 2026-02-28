// BUG 41: [MINOR] Implicit return (single-expression body) fails
//
// Functions with implicit return (no `return` keyword) produce wrong
// exit code. The expression is evaluated but the result is not returned.
//
// EXPECTED: prints 25
// ACTUAL: compiles but exits with code 48 instead of correct result
//
// WORKAROUND: Always use explicit `return` keyword.
//
// Build: tinyswift compile --no-prelude-import bugs/bug_41_minor_implicit_return.swift
// Run:   ./bug_41_minor_implicit_return

func square(_ x: Int) -> Int {
  x * x  // No `return` keyword - implicit return
}

func main() -> Int {
  print(square(5))  // Should print 25
  return 0
}
