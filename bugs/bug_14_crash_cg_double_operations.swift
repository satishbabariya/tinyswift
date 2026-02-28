// BUG 14: [CRASH-CG] Double operations crash codegen
//
// Operations involving Double values crash with LLVM cast assertion.
// Double comparisons work, but arithmetic and print(Double) crash.
//
// EXPECTED: prints 3.14
// ACTUAL: CRASH: "Invalid cast!"
//
// Also crashes: Double + Double, Double(intValue)
// Works: Double comparison (3.14 > 2.0)
//
// Build: tinyswift compile --no-prelude-import bugs/bug_14_crash_cg_double_operations.swift

func main() -> Int {
  let x: Double = 3.14
  print(x)  // CRASH: "Invalid cast!"
  return 0
}
