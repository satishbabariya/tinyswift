// BUG 81: [CRASH-CG] inout with array element crashes codegen
//
// Passing an array element by inout reference (`&arr[i]`) crashes
// the compiler. Passing plain variables by inout works fine.
//
// EXPECTED: compiles and prints 21
// ACTUAL: compiler crash (LLVM function signature assertion)
//
// Build: tinyswift compile --no-prelude-import bugs/bug_81_crash_cg_inout_array_elem.swift

func increment(_ x: inout Int) {
  x = x + 1
  return
}

func main() -> Int {
  var arr: [Int] = [10, 20, 30]
  increment(&arr[1])     // COMPILER CRASH
  print(arr[1])
  return 0
}
