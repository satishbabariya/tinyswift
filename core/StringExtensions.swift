// Part of the TinySwift Core Prelude (M88).
// Extension methods on String.

@extern("C") func __tinyswift_string_hash(_ s: String) -> Int
@extern("C") func __tinyswift_string_compare(_ a: String, _ b: String) -> Int

extension String {
  func hashValue() -> Int {
    return __tinyswift_string_hash(self)
  }
}
