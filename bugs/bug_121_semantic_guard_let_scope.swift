// BUG 121: [SEMANTIC] Guard-let bound variable out of scope
//
// Variables bound in a guard-let statement are not available after the
// guard statement, though Swift requires they be in scope for the rest
// of the enclosing block.
//
// EXPECTED: compiles and prints 42 then 0
// ACTUAL: error: use of undefined name 'v'
//
// WORKAROUND: Use if-let with the body in the if clause.

func unwrap(_ x: Int?) -> Int {
  guard let v = x else { return 0 }
  return v   // ERROR: use of undefined name 'v'
}

func main() -> Int {
  print(unwrap(42))
  print(unwrap(nil))
  return 0
}
