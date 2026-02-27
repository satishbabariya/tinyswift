// Part of the TinySwift Core Prelude (M88).
// Extension methods on String.

@extern("C") func __tinyswift_string_hash(_ s: String) -> Int
@extern("C") func __tinyswift_string_compare(_ a: String, _ b: String) -> Int
@extern("C") func __tinyswift_string_index_of(_ s: String, _ sub: String) -> Int
@extern("C") func __tinyswift_string_replace(_ s: String, _ target: String, _ replacement: String) -> String
@extern("C") func __tinyswift_string_substring(_ s: String, _ from: Int, _ to: Int) -> String
@extern("C") func __tinyswift_string_to_int(_ s: String, _ out_success: Int) -> Int
@extern("C") func __tinyswift_string_to_double(_ s: String, _ out_success: Int) -> Double
@extern("C") func __tinyswift_string_repeated(_ s: String, _ count: Int) -> String

extension String {
  func hashValue() -> Int {
    return __tinyswift_string_hash(self)
  }

  func description() -> String {
    return self
  }

  func compare(_ other: String) -> Int {
    return __tinyswift_string_compare(self, other)
  }

  func replacingOccurrences(of target: String, with replacement: String) -> String {
    return __tinyswift_string_replace(self, target, replacement)
  }

  func indexOf(_ sub: String) -> Int {
    return __tinyswift_string_index_of(self, sub)
  }

  func substring(from: Int, to: Int) -> String {
    return __tinyswift_string_substring(self, from, to)
  }

  func toInt() -> Int {
    return __tinyswift_string_to_int(self, 0)
  }

  func toDouble() -> Double {
    return __tinyswift_string_to_double(self, 0)
  }

  func repeated(_ count: Int) -> String {
    return __tinyswift_string_repeated(self, count)
  }
}
