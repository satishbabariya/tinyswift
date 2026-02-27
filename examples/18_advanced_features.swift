// TinySwift Example: Advanced Features
// Demonstrates typealias, defer, subscript, inout, address-of, and more.

// --- Typealias ---

typealias Distance = Double
typealias Name = String
typealias Coordinate = (Int, Int)
typealias Predicate = (Int) -> Bool
typealias Transform = (Int) -> Int
typealias StringMap = [String: String]

func measureDistance(_ from: Coordinate, _ to: Coordinate) -> Distance {
  let dx: Int = to.0 - from.0
  let dy: Int = to.1 - from.1
  return sqrt(Double(dx * dx + dy * dy))
}

// --- Defer statement (cleanup on scope exit) ---

func processFile(_ name: String) -> Int {
  var result: Int = 0
  defer {
    // This runs when the function exits, regardless of how
    result = result + 0
  }
  result = 42
  return result
}

func multiDefer() -> Int {
  var log: Int = 0
  defer {
    log = log + 1  // runs third (LIFO order)
  }
  defer {
    log = log + 10  // runs second
  }
  defer {
    log = log + 100  // runs first
  }
  return log
}

// --- Inout parameters (pass by reference) ---

func increment(_ value: inout Int) -> Void {
  value = value + 1
}

func swap(_ a: inout Int, _ b: inout Int) -> Void {
  let temp: Int = a
  a = b
  b = temp
}

func normalize(_ x: inout Double, _ y: inout Double) -> Void {
  let length: Double = sqrt(x * x + y * y)
  x = x / length
  y = y / length
}

// --- Address-of operator (&) for inout ---
// Usage:
// var count: Int = 0
// increment(&count)  // count is now 1

// var a: Int = 1
// var b: Int = 2
// swap(&a, &b)  // a is 2, b is 1

// --- Subscript declaration ---

struct Matrix {
  var rows: Int
  var cols: Int
  var data: [Double]

  subscript(row: Int, col: Int) -> Double {
    return data[row * cols + col]
  }
}

struct SafeArray {
  var storage: [Int]

  subscript(index: Int) -> Int? {
    if index < 0 {
      return nil
    }
    return storage[index]
  }
}

// --- Property accessors (get/set) ---

struct Temperature {
  var celsius: Double

  var fahrenheit: Double {
    get {
      return celsius * 9.0 / 5.0 + 32.0
    }
    set {
      celsius = (newValue - 32.0) * 5.0 / 9.0
    }
  }
}

// --- Property observers (willSet/didSet) ---

struct StepCounter {
  var totalSteps: Int {
    willSet {
      // About to change
    }
    didSet {
      // Just changed
    }
  }
}

// --- Semicolons for multiple statements on one line ---

var a: Int = 1; var b: Int = 2; var c: Int = a + b

// --- Nested functions ---

func outerFunction(_ x: Int) -> Int {
  func innerDouble(_ n: Int) -> Int {
    return n * 2
  }
  func innerAdd(_ n: Int) -> Int {
    return n + 10
  }
  return innerAdd(innerDouble(x))
}
