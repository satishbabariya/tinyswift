// BUG 84: [CRASH-CG] Switch on tuple patterns crashes codegen
//
// Using a tuple as the switch subject crashes the compiler with LLVM
// ICmpInst type mismatch assertion.
//
// EXPECTED: compiles and prints "origin", "x-axis", "y-axis", "other"
// ACTUAL: compiler crash (LLVM type assertion)
//
// Build: tinyswift compile --no-prelude-import bugs/bug_84_crash_cg_switch_tuple.swift

func classify(_ x: Int, _ y: Int) -> String {
  switch (x, y) {                  // COMPILER CRASH
  case (0, 0): return "origin"
  case (_, 0): return "x-axis"
  case (0, _): return "y-axis"
  default: return "other"
  }
}

func main() -> Int {
  print(classify(0, 0))
  print(classify(5, 0))
  print(classify(0, 3))
  print(classify(1, 1))
  return 0
}
