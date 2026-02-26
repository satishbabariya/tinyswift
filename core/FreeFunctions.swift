// Part of the TinySwift Core Prelude (M88).
// Free functions available to all TinySwift programs.

func min(_ a: Int, _ b: Int) -> Int {
  return a < b ? a : b
}

func max(_ a: Int, _ b: Int) -> Int {
  return a > b ? a : b
}

func abs(_ x: Int) -> Int {
  return x < 0 ? 0 - x : x
}
