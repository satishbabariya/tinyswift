// Part of the TinySwift Core Prelude (M88).
// Extension methods on Int.

@extern("C") func __tinyswift_int_abs(_ x: Int) -> Int
@extern("C") func __tinyswift_int_clamp(_ x: Int, _ lo: Int, _ hi: Int) -> Int
@extern("C") func __tinyswift_int_hash(_ x: Int) -> Int
@extern("C") func __tinyswift_int_to_string(_ x: Int) -> String

extension Int {
  func abs() -> Int {
    return __tinyswift_int_abs(self)
  }

  func clamped(to lo: Int, _ hi: Int) -> Int {
    return __tinyswift_int_clamp(self, lo, hi)
  }

  func hashValue() -> Int {
    return __tinyswift_int_hash(self)
  }

  func isMultiple(of other: Int) -> Bool {
    return self % other == 0
  }

  func description() -> String {
    return __tinyswift_int_to_string(self)
  }
}
