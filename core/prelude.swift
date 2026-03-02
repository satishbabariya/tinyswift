// Part of the TinySwift Core Prelude.
// Provides essential extension methods for Bool and Int built-in types.
// These methods are automatically available to all TinySwift programs.

// --- Bool Extensions ---

extension Bool {
  func description() -> String {
    if self {
      return "true"
    }
    return "false"
  }

  func toggled() -> Bool {
    if self {
      return false
    }
    return true
  }
}

// --- Int Extensions ---

extension Int {
  func abs() -> Int {
    if self < 0 {
      return 0 - self
    }
    return self
  }

  func isMultiple(of other: Int) -> Bool {
    return self % other == 0
  }

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
