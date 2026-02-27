// Part of the TinySwift Core Prelude (M88).
// Extension methods on Double.

@extern("C") func __tinyswift_double_abs(_ x: Double) -> Double
@extern("C") func __tinyswift_double_hash(_ x: Double) -> Int
@extern("C") func __tinyswift_double_to_string(_ x: Double) -> String

extension Double {
  func abs() -> Double {
    return __tinyswift_double_abs(self)
  }

  func hashValue() -> Int {
    return __tinyswift_double_hash(self)
  }

  func description() -> String {
    return __tinyswift_double_to_string(self)
  }
}
