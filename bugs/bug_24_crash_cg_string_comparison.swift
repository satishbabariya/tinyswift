// BUG 24: [CRASH-CG] String comparison operators
//
// Comparing strings with == or < crashes LLVM IR.
//
// EXPECTED: compiles and prints result
// ACTUAL: CRASH during codegen
//
// Note: String switch works. Only direct comparison operators crash.
//
// Build: tinyswift compile --no-prelude-import bugs/bug_24_crash_cg_string_comparison.swift

func main() -> Int {
  let eq: Bool = "hello" == "hello"  // CRASH
  print(eq)
  return 0
}
