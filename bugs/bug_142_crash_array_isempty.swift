// BUG 142: [CRASH] Array.isEmpty crashes codegen
//
// Accessing .isEmpty on arrays causes a stack dump during compilation.
// String.isEmpty works (but prints -1/0 per Bug 38).
//
// EXPECTED: prints true then false
// ACTUAL: compiler crash (stack dump)
//
// WORKAROUND: Check arr.count == 0 instead of arr.isEmpty.

func main() -> Int {
  let a: [Int] = []
  let b: [Int] = [1]
  print(a.isEmpty)    // CRASH
  print(b.isEmpty)
  return 0
}
