// BUG 54: [CRASH-CG] Returning array from function crashes codegen
//
// Functions that build and return an array crash with SIL verification
// error ("return missing operand") followed by LLVM assertion.
//
// EXPECTED: compiles and prints "5\n5"
// ACTUAL: compiler crash (SIL + LLVM assertions)
//
// Build: tinyswift compile --no-prelude-import bugs/bug_54_crash_cg_array_return.swift

func makeArr(_ n: Int) -> [Int] {
  var arr: [Int] = [1]
  var i: Int = 2
  while i <= n {
    arr.append(i)
    i = i + 1
  }
  return arr    // COMPILER CRASH
}

func main() -> Int {
  let a: [Int] = makeArr(5)
  print(a.count)
  print(a[4])
  return 0
}
