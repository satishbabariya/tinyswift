// BUG 47: [CRASH-CG] Empty array literal crashes compiler
//
// Declaring an empty array literal `[]` causes the compiler to crash
// with an assertion error in SemIR (ErrorInst cast to ClassType).
//
// Non-empty array literals work fine.
//
// EXPECTED: compiles and prints 0
// ACTUAL: compiler crash with assertion:
//   "CHECK failure: Is<TypedInst>(): Casting inst {kind: ErrorInst, type: type(Error)} to wrong kind ClassType"
//
// WORKAROUND: Initialize with at least one element, or avoid empty arrays.
//
// Build: tinyswift compile --no-prelude-import bugs/bug_47_crash_cg_empty_array.swift

func main() -> Int {
  let arr: [Int] = []   // COMPILER CRASH
  print(arr.count)
  return 0
}
