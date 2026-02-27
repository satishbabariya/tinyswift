// Part of the TinySwift Core Prelude (M88).
// Free functions available to all TinySwift programs.

@extern("C") func __tinyswift_abort(_ message: String) -> Void
@extern("C") func __tinyswift_double_abs(_ x: Double) -> Double

func min(_ a: Int, _ b: Int) -> Int {
  return a < b ? a : b
}

func max(_ a: Int, _ b: Int) -> Int {
  return a > b ? a : b
}

func abs(_ x: Int) -> Int {
  return x < 0 ? 0 - x : x
}

func min(_ a: Double, _ b: Double) -> Double {
  if a < b {
    return a
  }
  return b
}

func max(_ a: Double, _ b: Double) -> Double {
  if a > b {
    return a
  }
  return b
}

func abs(_ x: Double) -> Double {
  return __tinyswift_double_abs(x)
}

func swap(_ a: inout Int, _ b: inout Int) -> Void {
  let temp: Int = a
  a = b
  b = temp
}

func fatalError(_ message: String) -> Void {
  __tinyswift_abort(message)
}

func precondition(_ condition: Bool, _ message: String) -> Void {
  if !condition {
    __tinyswift_abort(message)
  }
}
