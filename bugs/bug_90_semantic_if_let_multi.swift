// BUG 90: [SEMANTIC] if-let with comma-separated multiple bindings doesn't bind
//
// `if let a = x, let b = y { ... }` fails because the bound variables
// are not accessible in the body. Single if-let works.
//
// EXPECTED: prints 15
// ACTUAL: error: use of undefined name 'a'
//
// WORKAROUND: Use nested if-let statements.
//
// Build: tinyswift compile --no-prelude-import bugs/bug_90_semantic_if_let_multi.swift

func main() -> Int {
  let x: Int? = 5
  let y: Int? = 10
  if let a = x, let b = y {     // 'a' and 'b' not bound
    print(a + b)                 // ERROR: use of undefined name 'a'
  }
  return 0
}

// Workaround: nested if-let
// if let a = x {
//   if let b = y {
//     print(a + b)
//   }
// }
