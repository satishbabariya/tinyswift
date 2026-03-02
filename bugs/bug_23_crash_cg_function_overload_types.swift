// BUG 23: [CRASH-CG] Function overloads with different types
//
// Overloaded functions with different parameter types crash codegen.
//
// EXPECTED: compiles and runs
// ACTUAL: CRASH during codegen
//
// Note: Overloads with same types but different parameter counts work.
//
// Build: tinyswift compile --no-prelude-import bugs/bug_23_crash_cg_function_overload_types.swift

func double(_ x: Int) -> Int {
  return x * 2
}

func double(_ x: String) -> String {
  return x
}

func main() -> Int {
  print(double(5))
  return 0
}
