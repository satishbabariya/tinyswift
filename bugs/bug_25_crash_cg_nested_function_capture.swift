// BUG 25: [CRASH-CG] Nested function capturing outer variable
//
// Nested function accessing outer scope's parameter crashes codegen.
//
// EXPECTED: prints 20 (10 + 10)
// ACTUAL: CRASH during codegen
//
// Build: tinyswift compile --no-prelude-import bugs/bug_25_crash_cg_nested_function_capture.swift

func makeAdder(_ base: Int) -> Int {
  func add(_ x: Int) -> Int {
    return base + x  // Captures `base` from outer scope
  }
  return add(10)
}

func main() -> Int {
  print(makeAdder(10))
  return 0
}
