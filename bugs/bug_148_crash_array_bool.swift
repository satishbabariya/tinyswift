// BUG 148: [CRASH] Array of Bool crashes at runtime
//
// Creating a [Bool] array and accessing elements causes a segfault.
// Arrays of Int and String work correctly.
//
// EXPECTED: prints "first true"
// ACTUAL: segfault (rc=139)
//
// WORKAROUND: Use [Int] with 0/1 values instead of [Bool].

func main() -> Int {
  let arr: [Bool] = [true, false, true]
  if arr[0] { print("first true") }   // SEGFAULT
  return 0
}
