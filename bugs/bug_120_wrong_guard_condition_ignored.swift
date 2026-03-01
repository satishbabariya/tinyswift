// BUG 120: [WRONG] Guard else clause never taken
//
// Guard statements always succeed - the else clause is never entered
// regardless of the condition. `guard x > 10 else { return 0 }` always
// falls through even when x <= 10.
//
// EXPECTED: prints 1 then 0 then 0
// ACTUAL: prints 1 then 1 then 1 (guard always succeeds)
//
// WORKAROUND: Use if-else instead of guard.

func test(_ x: Int) -> Int {
  guard x > 10 else { return 0 }
  return 1
}

func main() -> Int {
  print(test(15))   // Expected: 1, Actual: 1 ✓
  print(test(5))    // Expected: 0, Actual: 1 ✗
  print(test(0 - 1)) // Expected: 0, Actual: 1 ✗
  return 0
}
