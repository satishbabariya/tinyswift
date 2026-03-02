// BUG 82: [CRASH-CG] Array of optionals crashes codegen
//
// Declaring an array containing optional values `[Int?]` crashes the
// compiler with LLVM InsertValueInst type mismatch assertion.
//
// EXPECTED: compiles and prints "1\n0\n3"
// ACTUAL: compiler crash (LLVM type assertion)
//
// Build: tinyswift compile --no-prelude-import bugs/bug_82_crash_cg_array_of_optionals.swift

func main() -> Int {
  let arr: [Int?] = [1, nil, 3]   // COMPILER CRASH
  print(arr[0] ?? 0)
  print(arr[1] ?? 0)
  print(arr[2] ?? 0)
  return 0
}
