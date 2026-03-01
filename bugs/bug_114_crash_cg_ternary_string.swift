// BUG 114: [CRASH-CG] Ternary operator with String type crashes codegen
//
// Using the ternary operator with String result type crashes with
// LLVM CallInst assertion "Calling a function with a bad signature!".
// Ternary with Int result type works correctly.
//
// EXPECTED: prints "big"
// ACTUAL: compiler crash (LLVM CallInst assertion)
//
// WORKAROUND: Use a function with sequential if-return statements:
//   func pick(_ x: Int) -> String {
//     if x > 3 { return "big" }
//     return "small"
//   }
//
// Build: tinyswift compile --no-prelude-import --output=OUT bugs/bug_114_crash_cg_ternary_string.swift

func main() -> Int {
  let x: Int = 5
  let s: String = x > 3 ? "big" : "small"   // CRASH
  print(s)
  return 0
}
