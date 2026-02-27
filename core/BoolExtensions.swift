// Part of the TinySwift Core Prelude (M88).
// Extension methods on Bool.

@extern("C") func __tinyswift_bool_hash(_ x: Bool) -> Int

extension Bool {
  func hashValue() -> Int {
    return __tinyswift_bool_hash(self)
  }

  func description() -> String {
    if self {
      return "true"
    }
    return "false"
  }
}
