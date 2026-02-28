// BUG 17: [CRASH-CG] Array concatenation (+) crashes codegen
//
// Using + to append to arrays crashes with LLVM assertion.
//
// EXPECTED: compiles and runs
// ACTUAL: CRASH: "Tried to create an integer operation on a non-integer type!"
//
// Note: Array literals and subscript access work. Only + crashes.
// Array elements after concat may also return garbage values.
//
// Build: tinyswift compile --no-prelude-import bugs/bug_17_crash_cg_array_concat.swift

func main() -> Int {
  var arr: [Int] = []
  arr = arr + [10]
  print(arr[0])
  return 0
}
