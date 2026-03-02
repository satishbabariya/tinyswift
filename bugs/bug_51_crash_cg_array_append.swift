// BUG 51: [CRASH-CG] Array.append() crashes codegen
//
// Calling .append() on a var array crashes the compiler with an LLVM
// assertion about function signature mismatch (wrong number of params).
//
// EXPECTED: compiles and prints "4\n4"
// ACTUAL: compiler crash (LLVM CallInst assertion)
//
// Build: tinyswift compile --no-prelude-import bugs/bug_51_crash_cg_array_append.swift

func main() -> Int {
  var arr: [Int] = [1, 2, 3]
  arr.append(4)       // COMPILER CRASH
  print(arr.count)
  print(arr[3])
  return 0
}
