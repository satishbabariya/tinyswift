// BUG 138: [CRASH] Array.append on non-empty array literal crashes codegen
//
// Calling .append() on an array initialized with literal elements crashes
// with CallInst assertion. Appending to initially-empty arrays works fine.
//
// EXPECTED: prints 4
// ACTUAL: Assertion "Calling a function with bad signature!"
//
// WORKAROUND: Initialize array as empty, then append all elements.
//   var arr: [Int] = []
//   arr.append(1); arr.append(2); arr.append(3); arr.append(4)

func main() -> Int {
  var arr: [Int] = [1, 2, 3]
  arr.append(4)              // CRASH
  print(arr.count)
  return 0
}
