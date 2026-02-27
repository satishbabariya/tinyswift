// TinySwift Example: Functions
// Demonstrates function declarations, parameters, return types, and overloading.

// --- Basic function ---

func greet() -> String {
  return "Hello, TinySwift!"
}

// --- Function with parameters ---

func add(_ a: Int, _ b: Int) -> Int {
  return a + b
}

// --- Named (external) parameter labels ---

func divide(dividend: Int, by divisor: Int) -> Int {
  return dividend / divisor
}

// Usage: divide(dividend: 10, by: 3)

// --- Mixed named and unnamed parameters ---

func repeat_string(_ s: String, times count: Int) -> String {
  return s.repeated(count)
}

// --- Void return type ---

func doNothing() -> Void {
  // This function returns nothing
}

// --- Single-line function body ---

func square(_ x: Int) -> Int { return x * x }
func double(_ x: Double) -> Double { return x * 2.0 }

// --- Function with multiple return paths ---

func absoluteValue(_ x: Int) -> Int {
  if x < 0 {
    return 0 - x
  }
  return x
}

// --- Function overloading (same name, different types) ---

func min(_ a: Int, _ b: Int) -> Int {
  return a < b ? a : b
}

func min(_ a: Double, _ b: Double) -> Double {
  if a < b {
    return a
  }
  return b
}

// --- Inout parameters (pass by reference) ---

func swap(_ a: inout Int, _ b: inout Int) -> Void {
  let temp: Int = a
  a = b
  b = temp
}

// Usage:
// var x: Int = 1
// var y: Int = 2
// swap(&x, &y)  // x is now 2, y is now 1

// --- Function with many parameters ---

func clamp(_ value: Int, minimum: Int, maximum: Int) -> Int {
  if value < minimum {
    return minimum
  }
  if value > maximum {
    return maximum
  }
  return value
}

// --- Functions that take function parameters ---

func apply(_ f: (Int) -> Int, to value: Int) -> Int {
  return f(value)
}

func compose(_ f: (Int) -> Int, _ g: (Int) -> Int) -> (Int) -> Int {
  return { (x: Int) -> Int in
    return f(g(x))
  }
}

// --- Recursive function ---

func factorial(_ n: Int) -> Int {
  if n <= 1 {
    return 1
  }
  return n * factorial(n - 1)
}

// --- Function with throws ---

func safeDivide(_ a: Int, by b: Int) throws -> Int {
  return a / b
}

// --- Async function ---

func fetchValue(_ id: Int) async -> Int {
  return id * 10
}
