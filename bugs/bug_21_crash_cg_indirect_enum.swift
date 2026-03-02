// BUG 21: [CRASH-CG] indirect enum crashes codegen
//
// Recursive enum types marked with `indirect` crash during codegen.
//
// EXPECTED: compiles and runs
// ACTUAL: CRASH during codegen
//
// Build: tinyswift compile --no-prelude-import bugs/bug_21_crash_cg_indirect_enum.swift

indirect enum List {
  case cons(Int, List)
  case nil_
}

func main() -> Int {
  let l: List = List.cons(1, List.cons(2, List.nil_))
  print(0)
  return 0
}
