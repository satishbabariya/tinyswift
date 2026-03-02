// BUG 160: [CRASH] Global variables and constants crash codegen
//
// Variables or constants declared at file scope (outside any function)
// crash with debug metadata errors or return type mismatches in the
// __tinyswift_init function. Both `var` and `let` at file scope crash.
//
// EXPECTED: prints 3
// ACTUAL: !dbg attachment points at wrong subprogram for __tinyswift_init
//
// WORKAROUND: Declare all variables inside functions or pass as parameters.
// Use static methods on structs for constant-like values.

var counter: Int = 0    // CRASH: global var

func increment() {
  counter = counter + 1
  return
}

func main() -> Int {
  increment()
  increment()
  increment()
  print(counter)
  return 0
}
