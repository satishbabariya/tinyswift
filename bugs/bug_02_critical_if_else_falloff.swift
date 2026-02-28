// BUG 2: [CRITICAL] if-else body without return falls off function
//
// Same as Bug 1 but for if-else. Both branches fall off instead of
// continuing to code after the if-else block.
//
// EXPECTED: prints 42
// ACTUAL: no output, exits with garbage code
//
// Build: tinyswift compile --no-prelude-import bugs/bug_02_critical_if_else_falloff.swift
// Run:   ./bug_02_critical_if_else_falloff

func main() -> Int {
  var result: Int = 0
  if 5 > 3 {
    result = 42
  } else {
    result = 99
  }
  print(result)  // NEVER REACHED
  return 0
}
