// BUG 129: [SEMANTIC] External parameter labels not recognized in calls
//
// Functions using different external and internal parameter names (like
// `func move(from start: Int, to end: Int)`) fail with "incorrect
// argument label" when called with the external name.
//
// EXPECTED: prints 10
// ACTUAL: error: incorrect argument label 'from' in call
//
// WORKAROUND: Use _ for external name or same name for both.

func move(from start: Int, to end: Int) -> Int {
  return end - start
}

func main() -> Int {
  print(move(from: 5, to: 15))   // ERROR
  return 0
}
