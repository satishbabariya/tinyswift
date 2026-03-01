// BUG 162: [WRONG] Implicit return (single-expression body) doesn't return
//
// Single-expression function bodies without explicit `return` keyword
// evaluate the expression but don't return the value. The function
// exits without producing a result.
//
// EXPECTED: prints 10
// ACTUAL: exit code 48 (no output, function falls off)
//
// WORKAROUND: Always use explicit `return` keyword.

func double(_ x: Int) -> Int { x * 2 }   // Evaluates but doesn't return

func main() -> Int {
  print(double(5))
  return 0
}
