// BUG 99: [CRASH-CG] Array.contains() crashes codegen
//
// Calling .contains() on an array crashes the compiler with LLVM
// function signature assertion.
//
// EXPECTED: prints "yes"
// ACTUAL: compiler crash (LLVM CallInst assertion)
//
// Build: tinyswift compile --no-prelude-import bugs/bug_99_crash_cg_array_contains.swift

func main() -> Int {
  let arr: [Int] = [1, 2, 3, 4, 5]
  if arr.contains(3) {       // COMPILER CRASH
    print("yes")
  }
  return 0
}
