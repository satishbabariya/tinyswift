// BUG 156: [SEMANTIC/CRASH] Function overloading fails
//
// Functions with the same name but different parameter counts or types
// are not resolved correctly. Overloading by arity picks the wrong
// overload; overloading by type crashes with CallInst assertion.
//
// EXPECTED: prints "int" then "str"
// ACTUAL (by type): Assertion "Calling a function with a bad signature!"
// ACTUAL (by count): error: too few arguments (picks wrong overload)
//
// WORKAROUND: Use unique function names for each variant.

// By type (crashes):
func show(_ x: Int) -> String { return "int" }
func show(_ x: String) -> String { return "str" }

// By count (picks wrong one):
// func add(_ a: Int) -> Int { return a }
// func add(_ a: Int, _ b: Int) -> Int { return a + b }
// add(1)  // ERROR: too few arguments (picks 2-param version)

func main() -> Int {
  print(show(42))    // CRASH
  print(show("hi"))
  return 0
}
