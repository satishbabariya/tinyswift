// BUG 116: [SEMANTIC] Struct methods cannot call free (top-level) functions
//
// A method defined inside a struct body cannot call a free function
// defined at the top level. The compiler reports "use of undefined name"
// even though the function is visible from other scopes.
//
// Struct methods CAN call other methods on `self`, and free functions
// CAN call other free functions. Only cross-scope (struct method →
// free function) fails.
//
// EXPECTED: prints 14
// ACTUAL: error: use of undefined name 'double'
//
// WORKAROUND: Avoid calling free functions from struct methods.
// Instead, call the free function before/after the method call
// and pass the result as a parameter.
//
// Build: tinyswift compile --no-prelude-import --output=OUT bugs/bug_116_semantic_struct_method_free_func.swift

func double(_ x: Int) -> Int { return x * 2 }

struct Box {
  var val: Int
  func doubled() -> Int { return double(self.val) }  // ERROR: undefined 'double'
}

func main() -> Int {
  let b: Box = Box(val: 7)
  print(b.doubled())
  return 0
}
