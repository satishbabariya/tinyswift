// BUG 117: [SEMANTIC] Function-typed local variables cannot be called
//
// Storing a named function reference in a local variable of function
// type compiles, but calling through that variable fails with
// "cannot call non-function value". Passing functions as parameters
// to other functions works correctly.
//
// EXPECTED: prints 10
// ACTUAL: error: cannot call non-function value
//
// WORKAROUND: Pass function references directly as arguments to
// other functions, or call the function by its original name.
//
// Build: tinyswift compile --no-prelude-import --output=OUT bugs/bug_117_semantic_func_var_not_callable.swift

func double(_ x: Int) -> Int { return x * 2 }

func main() -> Int {
  let f: (Int) -> Int = double
  print(f(5))    // ERROR: cannot call non-function value
  return 0
}

// WORKAROUND (calling directly works):
//   print(double(5))
//
// WORKAROUND (passing as parameter works):
//   func apply(_ f: (Int) -> Int, _ x: Int) -> Int { return f(x) }
//   print(apply(double, 5))
