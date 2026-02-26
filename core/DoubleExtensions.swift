// Part of the TinySwift Core Prelude (M88).
// Extension methods on Double.

@extern("C") func __tinyswift_double_abs(_ x: Double) -> Double
@extern("C") func __tinyswift_double_hash(_ x: Double) -> Int

extension Double {
  func abs() -> Double {
    return __tinyswift_double_abs(self)
  }

  func hashValue() -> Int {
    return __tinyswift_double_hash(self)
  }
}
