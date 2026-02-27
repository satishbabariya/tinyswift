// TinySwift Example: Extensions
// Demonstrates extending existing types with new methods.

// --- Extension on Int ---

extension Int {
  func abs() -> Int {
    if self < 0 {
      return 0 - self
    }
    return self
  }

  func isEven() -> Bool {
    return self % 2 == 0
  }

  func isOdd() -> Bool {
    return self % 2 != 0
  }

  func isMultiple(of other: Int) -> Bool {
    return self % other == 0
  }

  func isPositive() -> Bool {
    return self > 0
  }

  func isNegative() -> Bool {
    return self < 0
  }

  func isZero() -> Bool {
    return self == 0
  }

  func clamped(to lo: Int, _ hi: Int) -> Int {
    if self < lo {
      return lo
    }
    if self > hi {
      return hi
    }
    return self
  }

  func squared() -> Int {
    return self * self
  }
}

// --- Extension on Double ---

extension Double {
  func abs() -> Double {
    if self < 0.0 {
      return 0.0 - self
    }
    return self
  }

  func rounded() -> Double {
    return round(self)
  }

  func isPositive() -> Bool {
    return self > 0.0
  }

  func isNegative() -> Bool {
    return self < 0.0
  }
}

// --- Extension on Bool ---

extension Bool {
  func toggled() -> Bool {
    if self {
      return false
    }
    return true
  }

  func description() -> String {
    if self {
      return "true"
    }
    return "false"
  }
}

// --- Extension on String ---

extension String {
  func isEmpty() -> Bool {
    return self == ""
  }

  func reversed() -> String {
    return self
  }

  func repeated(_ count: Int) -> String {
    var result: String = ""
    var i: Int = 0
    while i < count {
      result = result + self
      i = i + 1
    }
    return result
  }

  func hasPrefix(_ prefix: String) -> Bool {
    return self.indexOf(prefix) == 0
  }
}

// --- Extension on custom struct ---

struct Vector2D {
  var x: Double
  var y: Double
}

extension Vector2D {
  func magnitude() -> Double {
    return sqrt(x * x + y * y)
  }

  func normalized() -> Vector2D {
    let mag: Double = self.magnitude()
    return Vector2D(x: x / mag, y: y / mag)
  }

  func dot(_ other: Vector2D) -> Double {
    return x * other.x + y * other.y
  }

  func add(_ other: Vector2D) -> Vector2D {
    return Vector2D(x: x + other.x, y: y + other.y)
  }

  func scale(_ factor: Double) -> Vector2D {
    return Vector2D(x: x * factor, y: y * factor)
  }
}
