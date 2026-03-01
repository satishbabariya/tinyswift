// BUG 152: [CRASH] `else if` chains segfault at runtime
//
// Functions with `else if` chains (3+ branches) compile but segfault.
// Simple `if/else` (2 branches) works correctly. Sequential if-return
// statements also work.
//
// EXPECTED: prints 1, -1, 0
// ACTUAL: segfault (rc=139)
//
// WORKAROUND: Use sequential if-return statements without else:
//   if x > 0 { return 1 }
//   if x < 0 { return -1 }
//   return 0

func test(_ x: Int) -> Int {
  if x > 0 {
    return 1
  } else if x < 0 {         // SEGFAULT when this branch runs
    return 0 - 1
  } else {
    return 0
  }
}

func main() -> Int {
  print(test(5))
  print(test(0 - 3))
  print(test(0))
  return 0
}
