// BUG 107: [CRASH-CG] Struct with array field crashes codegen
//
// Defining a struct that contains an array field (e.g., `var scores: [Int]`)
// crashes with LLVM InsertValueInst type assertion when constructing it.
//
// Structs with only scalar fields work fine. Only the presence of an
// array-typed field causes the crash.
//
// EXPECTED: compiles and prints "Alpha\n10\n3"
// ACTUAL: compiler crash (LLVM type assertion)
//
// Build: tinyswift compile --no-prelude-import bugs/bug_107_crash_cg_struct_array_field.swift

struct Team {
  var name: String
  var scores: [Int]        // Array field causes crash
}

func main() -> Int {
  let t: Team = Team(name: "Alpha", scores: [10, 20, 30])
  print(t.name)
  print(t.scores[0])
  print(t.scores.count)
  return 0
}
