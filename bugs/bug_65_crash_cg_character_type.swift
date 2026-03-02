// BUG 65: [CRASH-CG] Character type crashes codegen
//
// Assigning a Character literal crashes the compiler with an LLVM
// CastInst assertion ("Invalid cast!").
//
// EXPECTED: compiles and prints "A"
// ACTUAL: compiler crash (LLVM cast assertion)
//
// Build: tinyswift compile --no-prelude-import bugs/bug_65_crash_cg_character_type.swift

func main() -> Int {
  let c: Character = "A"   // COMPILER CRASH
  print(c)
  return 0
}
