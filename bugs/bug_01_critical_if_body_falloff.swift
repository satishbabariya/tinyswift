// BUG 1: [CRITICAL] if-body without return falls off function
//
// When an if-body does NOT contain a `return` statement, execution
// falls off the end of the function instead of continuing to the
// code after the if. This is the root cause of many downstream bugs.
//
// ROOT CAUSE: LLVM IR codegen doesn't generate a branch from the
// then-block back to the continuation block.
//
// EXPECTED: prints 42
// ACTUAL: no output, exits with garbage code
//
// IMPACT: Any program using if-without-return is broken.
// WORKAROUND: Ensure every if-body ends with `return`.
//
// Build: tinyswift compile --no-prelude-import bugs/bug_01_critical_if_body_falloff.swift
// Run:   ./bug_01_critical_if_body_falloff

func main() -> Int {
  var result: Int = 0
  if true {
    result = 42
  }
  print(result)  // NEVER REACHED
  return 0
}
