// Part of the TinySwift Core Prelude (M88).
// Protocol declarations for the standard library.

protocol Hashable {
  func hashValue() -> Int
}

protocol CustomStringConvertible {
  func description() -> String
}
