// BUG 72: [CRASH-CG] Named tuple elements crash codegen
//
// Tuples with named elements (e.g., `(sum: Int, diff: Int)`) crash the
// compiler during codegen. Unnamed tuple elements (accessed via .0, .1)
// work (though with wrong values per Bug 7).
//
// EXPECTED: compiles and prints "13\n7"
// ACTUAL: compiler crash
//
// Build: tinyswift compile --no-prelude-import bugs/bug_72_crash_cg_named_tuple.swift

func stats(_ a: Int, _ b: Int) -> (sum: Int, diff: Int) {
  return (sum: a + b, diff: a - b)    // COMPILER CRASH
}

func main() -> Int {
  let r = stats(10, 3)
  print(r.sum)
  print(r.diff)
  return 0
}
