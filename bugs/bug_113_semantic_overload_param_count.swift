// BUG 113: [SEMANTIC] Function overloading by parameter count broken
//
// When two or more functions share the same name but differ in parameter
// count, the compiler always resolves to one overload (typically the
// last/largest) and ignores others. Calling with fewer arguments gives
// "too few arguments" error; calling with matching arguments to the
// non-selected overload crashes codegen.
//
// EXPECTED: prints 5 then 15
// ACTUAL: "too few arguments: expected 2, got 1" for f(5),
//         codegen crash for f(5, 10) when 1-param overload also exists
//
// WORKAROUND: Use different function names (e.g., add1, add2, add3).
//
// Build: tinyswift compile --no-prelude-import --output=OUT bugs/bug_113_semantic_overload_param_count.swift

func add(_ a: Int) -> Int { return a }
func add(_ a: Int, _ b: Int) -> Int { return a + b }

func main() -> Int {
  print(add(5))      // ERROR: too few arguments: expected 2, got 1
  print(add(5, 10))  // CRASH if both overloads exist
  return 0
}
