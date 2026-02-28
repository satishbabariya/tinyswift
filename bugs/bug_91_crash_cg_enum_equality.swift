// BUG 91: [CRASH-CG] Enum equality comparison (==, !=) crashes
//
// Comparing two enum values with == or != crashes the compiler with
// LLVM ICmpInst assertion about invalid operand types.
//
// EXPECTED: prints "same"
// ACTUAL: compiler crash (LLVM ICmp type assertion)
//
// WORKAROUND: Use switch to compare enum values manually.
//
// Build: tinyswift compile --no-prelude-import bugs/bug_91_crash_cg_enum_equality.swift

enum Color {
  case red
  case blue
}

func main() -> Int {
  let a: Color = Color.red
  let b: Color = Color.red
  if a == b {             // COMPILER CRASH
    print("same")
  }
  return 0
}
