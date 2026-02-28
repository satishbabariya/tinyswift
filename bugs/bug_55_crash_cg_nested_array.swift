// BUG 55: [CRASH-CG] Nested array type [[Int]] crashes codegen
//
// Declaring a nested array (array of arrays) crashes the compiler with
// an LLVM assertion about pointer types.
//
// EXPECTED: compiles and prints 1, 2, 3, 4
// ACTUAL: compiler crash (LLVM LoadInst pointer type assertion)
//
// Build: tinyswift compile --no-prelude-import bugs/bug_55_crash_cg_nested_array.swift

func main() -> Int {
  let a: [[Int]] = [[1, 2], [3, 4]]   // COMPILER CRASH
  print(a[0][0])
  print(a[0][1])
  print(a[1][0])
  print(a[1][1])
  return 0
}
