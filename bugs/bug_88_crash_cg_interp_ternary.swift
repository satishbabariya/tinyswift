// BUG 88: [CRASH-CG] Ternary expression inside string interpolation crashes
//
// Using a ternary operator inside `\(...)` string interpolation crashes
// the compiler with LLVM StringRef assertion.
//
// Related to Bug 44 (expressions in interpolation are empty) but this
// specific pattern causes a compiler crash rather than wrong output.
//
// EXPECTED: prints "value is big"
// ACTUAL: compiler crash (LLVM StringRef drop_front assertion)
//
// Build: tinyswift compile --no-prelude-import bugs/bug_88_crash_cg_interp_ternary.swift

func main() -> Int {
  let x: Int = 5
  print("value is \(x > 3 ? "big" : "small")")   // COMPILER CRASH
  return 0
}
