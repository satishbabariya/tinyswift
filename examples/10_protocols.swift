// TinySwift Example: Protocols
// Demonstrates protocol declarations, conformance, and Self type.

// --- Basic protocol ---

protocol Describable {
  func description() -> String
}

// --- Protocol with multiple requirements ---

protocol Hashable {
  func hashValue() -> Int
}

protocol Equatable {
  func equals(_ other: Self) -> Bool
}

protocol Comparable {
  func lessThan(_ other: Self) -> Bool
}

// --- Protocol with initializer ---

protocol DefaultInitializable {
  init()
}

// --- Protocol combining multiple concepts ---

protocol CustomStringConvertible {
  func description() -> String
}

// --- Structs conforming to protocols ---

struct Coordinate: Equatable {
  var x: Int
  var y: Int

  func equals(_ other: Coordinate) -> Bool {
    return x == other.x && y == other.y
  }
}

struct Size: Hashable {
  var width: Int
  var height: Int

  func hashValue() -> Int {
    return width * 31 + height
  }
}

struct Animal: CustomStringConvertible {
  var species: String
  var legs: Int

  func description() -> String {
    return species
  }
}

// --- Struct conforming to multiple protocols ---

struct Pixel: Equatable, Hashable, CustomStringConvertible {
  var x: Int
  var y: Int
  var color: Int

  func equals(_ other: Pixel) -> Bool {
    return x == other.x && y == other.y && color == other.color
  }

  func hashValue() -> Int {
    return x * 10000 + y * 100 + color
  }

  func description() -> String {
    return "Pixel"
  }
}

// --- Enum conforming to a protocol ---

enum Suit: Hashable {
  case hearts
  case diamonds
  case clubs
  case spades

  func hashValue() -> Int {
    switch self {
    case .hearts: return 0
    case .diamonds: return 1
    case .clubs: return 2
    case .spades: return 3
    }
  }
}

// --- Class conforming to a protocol ---

class Vehicle: CustomStringConvertible {
  var make: String
  var model: String

  init(make: String, model: String) {
    self.make = make
    self.model = model
  }

  func description() -> String {
    return make
  }
}

// --- Protocol as function parameter type ---

func printDescription(_ item: CustomStringConvertible) -> String {
  return item.description()
}

func areSame(_ a: Equatable, _ b: Equatable) -> Bool {
  return a.equals(b)
}
