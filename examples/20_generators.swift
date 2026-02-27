// TinySwift Example: Generators
// Demonstrates Generator type, yield statements, and for-in consumption.

// --- Basic generator function ---

func oneToThree() -> Generator<Int> {
  yield 1
  yield 2
  yield 3
}

// --- Generator with a loop ---

func countUpTo(_ n: Int) -> Generator<Int> {
  var i: Int = 1
  while i <= n {
    yield i
    i = i + 1
  }
}

// --- Infinite generator ---

func naturals() -> Generator<Int> {
  var n: Int = 1
  while true {
    yield n
    n = n + 1
  }
}

// --- Fibonacci generator ---

func fibonacci() -> Generator<Int> {
  var a: Int = 0
  var b: Int = 1
  while true {
    yield a
    let tmp: Int = a
    a = b
    b = tmp + b
  }
}

// --- Generator with conditional yields ---

func evens(_ limit: Int) -> Generator<Int> {
  var i: Int = 0
  while i <= limit {
    yield i
    i = i + 2
  }
}

func odds(_ limit: Int) -> Generator<Int> {
  var i: Int = 1
  while i <= limit {
    yield i
    i = i + 2
  }
}

// --- Consuming generators with for-in ---

func sumFirstN(_ n: Int) -> Int {
  var total: Int = 0
  var count: Int = 0
  for value in naturals() {
    if count >= n {
      break
    }
    total = total + value
    count = count + 1
  }
  return total
}

func firstNFib(_ n: Int) -> Int {
  var result: Int = 0
  var count: Int = 0
  for fib in fibonacci() {
    if count >= n {
      break
    }
    result = fib
    count = count + 1
  }
  return result
}

// --- Consuming generators manually with .next() ---

func manualConsume() -> Int {
  let gen: Generator<Int> = oneToThree()
  let a: Int = gen.next()!
  let b: Int = gen.next()!
  let c: Int = gen.next()!
  return a + b + c  // 6
}

// --- Generator of doubles ---

func powers(of base: Double, count: Int) -> Generator<Double> {
  var current: Double = 1.0
  var i: Int = 0
  while i < count {
    yield current
    current = current * base
    i = i + 1
  }
}

// --- Generator producing strings ---

func greetings(_ names: [String]) -> Generator<String> {
  for name in names {
    yield "Hello, " + name
  }
}
