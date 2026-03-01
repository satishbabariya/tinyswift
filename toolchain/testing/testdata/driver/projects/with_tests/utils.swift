// Utility functions for testing the test subcommand.

func isEven(_ n: Int) -> Bool {
  return n % 2 == 0
}

func max(_ a: Int, _ b: Int) -> Int {
  if a > b {
    return a
  }
  return b
}

func clamp(_ value: Int, _ low: Int, _ high: Int) -> Int {
  if value < low {
    return low
  }
  if value > high {
    return high
  }
  return value
}
