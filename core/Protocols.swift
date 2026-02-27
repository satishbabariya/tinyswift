// Part of the TinySwift Core Prelude (M88).
// Protocol declarations for the standard library.

protocol Hashable {
  func hashValue() -> Int
}

protocol CustomStringConvertible {
  func description() -> String
}

protocol Equatable {
  func equals(_ other: Self) -> Bool
}

protocol Comparable {
  func lessThan(_ other: Self) -> Bool
}

protocol DefaultInitializable {
  init()
}
