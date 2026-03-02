// BUG 22: [CRASH-CG] Closure expressions crash codegen
//
// Closure literals crash the compiler during codegen.
//
// EXPECTED: prints 30
// ACTUAL: CRASH during codegen
//
// Also crashes when passed as function argument.
// WORKAROUND: Use named functions instead.
//
// Build: tinyswift compile --no-prelude-import bugs/bug_22_crash_cg_closures.swift

func main() -> Int {
  let add: (Int, Int) -> Int = { (a: Int, b: Int) -> Int in
    return a + b
  }
  print(add(10, 20))
  return 0
}
