// TinySwift Example: Extensions
// Demonstrates extending existing types with new methods.
// Note: The prelude already defines abs(), clamped(), isMultiple(),
// hashValue(), description(), repeated(), etc. This example adds only
// NEW methods to demonstrate the extension syntax.

// --- Extension on Int (new methods only) ---

extension Int {
  func isEven() -> Bool {
    return self % 2 == 0
  }

  func isOdd() -> Bool {
    return self % 2 != 0
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

  func squared() -> Int {
    return self * self
  }

  func cubed() -> Int {
    return self * self * self
  }
}

// --- Extension on Double (new methods only) ---

extension Double {
  func isPositive() -> Bool {
    return self > 0.0
  }

  func isNegative() -> Bool {
    return self < 0.0
  }

  func isZero() -> Bool {
    return self == 0.0
  }

  func squared() -> Double {
    return self * self
  }
}

// --- Extension on Bool (new methods only) ---

extension Bool {
  func toggled() -> Bool {
    if self {
      return false
    }
    return true
  }

  func toInt() -> Int {
    if self {
      return 1
    }
    return 0
  }
}

// --- Extension on String (new methods only) ---

extension String {
  func isEmpty() -> Bool {
    return self == ""
  }

  func hasPrefix(_ prefix: String) -> Bool {
    return self.indexOf(prefix) == 0
  }

  func upper() -> String {
    return self  // placeholder — no runtime support yet
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
