// BUG 98: [CRASH-CG] Array.first / Array.last crash codegen
//
// Accessing .first or .last properties on arrays crashes the compiler.
//
// EXPECTED: prints 10
// ACTUAL: compiler crash
//
// WORKAROUND: Use arr[0] for first, arr[arr.count - 1] for last.
//
// Build: tinyswift compile --no-prelude-import bugs/bug_98_crash_cg_array_first_last.swift

func main() -> Int {
  let arr: [Int] = [10, 20, 30]
  print(arr.first ?? 0)     // COMPILER CRASH
  return 0
}

// Also crashes:
// print(arr.last ?? 0)
