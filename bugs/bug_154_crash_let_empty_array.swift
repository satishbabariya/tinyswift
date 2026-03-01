// BUG 154: [CRASH] Empty array literal with `let` crashes codegen
//
// `let arr: [Int] = []` causes a stack dump during compilation.
// `var arr: [Int] = []` works correctly (and allows append).
//
// EXPECTED: prints 0
// ACTUAL: compiler crash (stack dump)
//
// WORKAROUND: Use `var` instead of `let` for empty arrays.

func main() -> Int {
  let arr: [Int] = []      // CRASH
  print(arr.count)
  return 0
}
