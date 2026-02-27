// TinySwift Example: Closures
// Demonstrates closure syntax, higher-order functions, and function types.

// --- Basic closure expression ---

let addOne: (Int) -> Int = { (x: Int) -> Int in
  return x + 1
}

let multiply: (Int, Int) -> Int = { (a: Int, b: Int) -> Int in
  return a * b
}

// --- Closures as function parameters ---

func applyToInt(_ value: Int, _ transform: (Int) -> Int) -> Int {
  return transform(value)
}

func applyToDouble(_ value: Double, _ transform: (Double) -> Double) -> Double {
  return transform(value)
}

// --- Higher-order functions ---

func makeAdder(_ amount: Int) -> (Int) -> Int {
  return { (x: Int) -> Int in
    return x + amount
  }
}

func makeMultiplier(_ factor: Double) -> (Double) -> Double {
  return { (x: Double) -> Double in
    return x * factor
  }
}

// --- Function composition with closures ---

func compose(_ f: (Int) -> Int, _ g: (Int) -> Int) -> (Int) -> Int {
  return { (x: Int) -> Int in
    return f(g(x))
  }
}

// --- Passing closures to generic functions ---

func map<T, U>(_ value: T, _ transform: (T) -> U) -> U {
  return transform(value)
}

// --- Closure capturing values ---

func makeCounter() -> () -> Int {
  var count: Int = 0
  return { () -> Int in
    count = count + 1
    return count
  }
}

// --- Closures with multiple parameters ---

func applyBinary(_ a: Int, _ b: Int, _ op: (Int, Int) -> Int) -> Int {
  return op(a, b)
}

// Usage examples:
// applyBinary(3, 4, { (a: Int, b: Int) -> Int in return a + b })
// applyBinary(3, 4, { (a: Int, b: Int) -> Int in return a * b })

// --- Function type as struct field ---

struct Transformer {
  var transform: (Int) -> Int

  func apply(_ value: Int) -> Int {
    return transform(value)
  }
}

// --- Predicate closures ---

func filter(_ items: [Int], _ predicate: (Int) -> Bool) -> [Int] {
  var result: [Int] = []
  for item in items {
    if predicate(item) {
      // include item
    }
  }
  return result
}

func any(_ items: [Int], _ predicate: (Int) -> Bool) -> Bool {
  for item in items {
    if predicate(item) {
      return true
    }
  }
  return false
}

func all(_ items: [Int], _ predicate: (Int) -> Bool) -> Bool {
  for item in items {
    if !predicate(item) {
      return false
    }
  }
  return true
}
