// TinySwift Example: Structs
// Demonstrates struct definitions, properties, methods, and protocol conformance.

// --- Basic struct ---

struct Point {
  var x: Int
  var y: Int
}

// Usage: let p = Point(x: 10, y: 20)

// --- Struct with methods ---

struct Rectangle {
  var width: Int
  var height: Int

  func area() -> Int {
    return width * height
  }

  func perimeter() -> Int {
    return 2 * (width + height)
  }

  func isSquare() -> Bool {
    return width == height
  }
}

// --- Struct with computed results ---

struct Circle {
  var radius: Double

  func area() -> Double {
    return 3.14159265358979 * radius * radius
  }

  func circumference() -> Double {
    return 2.0 * 3.14159265358979 * radius
  }

  func diameter() -> Double {
    return 2.0 * radius
  }
}

// --- Struct returning new instances (value semantics) ---

struct Counter {
  var count: Int

  func increment() -> Counter {
    return Counter(count: count + 1)
  }

  func decrement() -> Counter {
    return Counter(count: count - 1)
  }

  func reset() -> Counter {
    return Counter(count: 0)
  }

  func isZero() -> Bool {
    return count == 0
  }
}

// --- Struct with protocol conformance ---

struct Temperature: Equatable {
  var degrees: Double
  var scale: String

  func equals(_ other: Temperature) -> Bool {
    return degrees == other.degrees && scale == other.scale
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

// --- Nested struct usage ---

struct Line {
  var start: Point
  var end: Point

  func length() -> Double {
    let dx: Int = end.x - start.x
    let dy: Int = end.y - start.y
    return sqrt(Double(dx * dx + dy * dy))
  }
}

// --- Struct with multiple methods ---

struct Stack {
  var items: [Int]

  func isEmpty() -> Bool {
    return items == []
  }

  func peek() -> Int? {
    if items == [] {
      return nil
    }
    return items[0]
  }
}
