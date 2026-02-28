// BUG 13: [CRASH-CG] Mixed-type enum associated values
//
// Enum cases with different associated value types crash LLVM IR
// during codegen with assertion error.
//
// EXPECTED: compiles and runs
// ACTUAL: LLVM IR assertion: "Initializer for struct element doesn't match!"
//
// WORKAROUND: Use same type for all associated values (e.g., all Int).
//
// Build: tinyswift compile --no-prelude-import bugs/bug_13_crash_cg_mixed_type_enum.swift

enum Value {
  case integer(Int)
  case text(String)
}

func main() -> Int {
  let v: Value = Value.integer(42)
  print(42)
  return 0
}
