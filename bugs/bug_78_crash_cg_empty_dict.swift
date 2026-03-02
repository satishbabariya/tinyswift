// BUG 78: [CRASH-CG] Empty dictionary literal crashes compiler
//
// Empty dictionary literal `[:]` crashes the compiler. Non-empty
// dictionary literals with String keys work.
//
// EXPECTED: compiles and prints 0
// ACTUAL: compiler crash
//
// Build: tinyswift compile --no-prelude-import bugs/bug_78_crash_cg_empty_dict.swift

func main() -> Int {
  let d: [String: Int] = [:]    // COMPILER CRASH
  print(d.count)
  return 0
}
