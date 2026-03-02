// BUG 28: [SEMANTIC] guard let - bound variable undefined
//
// Variables bound in guard-let are not accessible in the body
// after the guard statement.
//
// EXPECTED: compiles and prints 42
// ACTUAL: ERROR: use of undefined name 'val'
//
// Build: tinyswift compile --no-prelude-import bugs/bug_28_semantic_guard_let.swift

func test(_ x: Int?) -> Int {
  guard let val = x else { return 0 }
  return val  // ERROR: use of undefined name 'val'
}

func main() -> Int {
  print(test(42))
  return 0
}
