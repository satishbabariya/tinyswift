// BUG 115: [CRASH-CG] Array subscript on function parameter crashes
//
// Accessing array elements by index (arr[i]) when the array is a
// function parameter crashes with LLVM LoadInst assertion:
// "Ptr must have pointer type."
//
// Local array subscript access works fine. Only parameter arrays crash.
//
// EXPECTED: prints 10
// ACTUAL: compiler crash (LLVM LoadInst assertion)
//
// WORKAROUND: Copy array contents into local variables before indexing,
// or restructure to avoid passing arrays as parameters.
//
// Build: tinyswift compile --no-prelude-import --output=OUT bugs/bug_115_crash_cg_array_param_subscript.swift

func first(_ arr: [Int]) -> Int {
  return arr[0]   // CRASH: LoadInst "Ptr must have pointer type"
}

func main() -> Int {
  let a: [Int] = [10, 20, 30]
  print(first(a))
  return 0
}
