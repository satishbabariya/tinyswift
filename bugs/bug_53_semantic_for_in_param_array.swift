// BUG 53: [SEMANTIC] for-in loop variable unbound when iterating function parameter
//
// When a function receives an array parameter and iterates over it with
// for-in, the loop variable is not bound. Iterating over a local let
// array variable works fine.
//
// EXPECTED: compiles and prints 15
// ACTUAL: error: use of undefined name 'x'
//
// WORKAROUND: None found. Cannot iterate over array parameters.
//
// Build: tinyswift compile --no-prelude-import bugs/bug_53_semantic_for_in_param_array.swift

func sum(_ arr: [Int]) -> Int {
  var total: Int = 0
  for x in arr {          // 'x' is not bound
    total = total + x     // ERROR: use of undefined name 'x'
  }
  return total
}

func main() -> Int {
  let a: [Int] = [1, 2, 3, 4, 5]
  print(sum(a))
  return 0
}
