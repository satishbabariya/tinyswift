// BUG 62: [CRITICAL] `as` type cast expression produces no output
//
// Using `x as Type` compiles but the result appears to be empty/wrong.
// Printing the cast result produces no output, while printing the
// original value works fine.
//
// EXPECTED: prints "42" twice
// ACTUAL: prints "42" once (second print produces nothing)
//
// Build: tinyswift compile --no-prelude-import bugs/bug_62_critical_as_cast_silent.swift
// Run:   ./bug_62_critical_as_cast_silent

func main() -> Int {
  let x: Int = 42
  print(x)            // Prints 42

  let y: Int = x as Int
  print(y)            // Prints nothing

  return 0
}
