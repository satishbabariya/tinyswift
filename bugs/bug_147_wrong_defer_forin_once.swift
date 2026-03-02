// BUG 147: [WRONG] Defer in for-in loop fires once, not per iteration
//
// A defer statement inside a for-in loop body should execute at the end
// of each iteration. Instead, it only fires once after the entire loop.
//
// EXPECTED: 1, done, 2, done, 3, done
// ACTUAL: 1, 2, 3, done
//
// WORKAROUND: Call cleanup logic directly before loop end / continue.

func main() -> Int {
  for i in 1...3 {
    defer { print("done") }
    print(i)
  }
  return 0
}
