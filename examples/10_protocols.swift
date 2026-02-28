// TinySwift Example: Protocols
// Demonstrates protocol declarations, conformance, and Self type.
// Note: The prelude provides Hashable, Equatable, Comparable,
// DefaultInitializable, and CustomStringConvertible. This example defines
// distinct protocols to demonstrate the syntax, then also shows conformance
// to the prelude protocols.

// --- Basic protocol ---

protocol Describable {
  func describe() -> String
}

// --- Protocol with multiple requirements ---

protocol Hashing {
  func hash() -> Int
}

protocol Equating {
  func isEqual(_ other: Self) -> Bool
}

protocol Ordering {
  func isLessThan(_ other: Self) -> Bool
}

// --- Protocol with initializer ---

protocol Defaultable {
  init()
}

// --- Protocol combining multiple concepts ---

protocol Displayable {
  func display() -> String
}

// --- Structs conforming to user-defined protocols ---

struct Coordinate: Equating {
  var x: Int
  var y: Int

  func isEqual(_ other: Coordinate) -> Bool {
    return x == other.x && y == other.y
  }
}

struct Size: Hashing {
  var width: Int
  var height: Int

  func hash() -> Int {
    return width * 31 + height
  }
}

struct Animal: Displayable {
  var species: String
  var legs: Int

  func display() -> String {
    return species
  }
}

// --- Struct conforming to multiple protocols ---

struct Pixel: Equating, Hashing, Displayable {
  var x: Int
  var y: Int
  var color: Int

  func isEqual(_ other: Pixel) -> Bool {
    return x == other.x && y == other.y && color == other.color
  }

  func hash() -> Int {
    return x * 10000 + y * 100 + color
  }

  func display() -> String {
    return "Pixel"
  }
}

// --- Enum conforming to a protocol ---

enum Suit: Hashing {
  case hearts
  case diamonds
  case clubs
  case spades

  func hash() -> Int {
    switch self {
    case .hearts: return 0
    case .diamonds: return 1
    case .clubs: return 2
    case .spades: return 3
    }
  }
}

// --- Class conforming to a protocol ---

class Vehicle: Displayable {
  var make: String
  var model: String

  init(make: String, model: String) {
    self.make = make
    self.model = model
  }

  func display() -> String {
    return make
  }
}

// --- Conforming to prelude protocols ---

struct Temperature: Equatable {
  var degrees: Double

  func equals(_ other: Temperature) -> Bool {
    return degrees == other.degrees
  }
}

struct Color: Hashable {
  var r: Int
  var g: Int
  var b: Int

  func hashValue() -> Int {
    return r * 65536 + g * 256 + b
  }
}

struct Person: CustomStringConvertible {
  var name: String
  var age: Int

  func description() -> String {
    return name
  }
}

// --- Protocol as function parameter type ---

func printDisplay(_ item: Displayable) -> String {
  return item.display()
}
