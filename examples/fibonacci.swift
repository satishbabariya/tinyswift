// TinySwift Algorithmic Example: Fibonacci Sequence
// Demonstrates: recursion, iteration, print(), main entry point.
// Build: tinyswift compile examples/fibonacci.swift
// Run:   ./fibonacci

// --- Recursive Fibonacci ---

func fibRecursive(_ n: Int) -> Int {
  if n <= 0 {
    return 0
  }
  if n == 1 {
    return 1
  }
  return fibRecursive(n - 1) + fibRecursive(n - 2)
}

// --- Iterative Fibonacci ---

func fibIterative(_ n: Int) -> Int {
  if n <= 0 {
    return 0
  }
  if n == 1 {
    return 1
  }
  var a: Int = 0
  var b: Int = 1
  var i: Int = 2
  while i <= n {
    let temp: Int = a + b
    a = b
    b = temp
    i = i + 1
  }
  return b
}

// --- Fibonacci using Generator ---

func fibGenerator() -> Generator<Int> {
  var a: Int = 0
  var b: Int = 1
  while true {
    yield a
    let tmp: Int = a
    a = b
    b = tmp + b
  }
}

// --- Check if a number is a Fibonacci number ---

func isFibonacci(_ n: Int) -> Bool {
  var a: Int = 0
  var b: Int = 1
  while a < n {
    let temp: Int = a + b
    a = b
    b = temp
  }
  return a == n
}

// --- Sum of first N Fibonacci numbers ---

func fibSum(_ n: Int) -> Int {
  var total: Int = 0
  var a: Int = 0
  var b: Int = 1
  var i: Int = 0
  while i < n {
    total = total + a
    let temp: Int = a + b
    a = b
    b = temp
    i = i + 1
  }
  return total
}

// --- Entry point ---

func main() -> Int {
  // Print first 20 Fibonacci numbers (iterative)
  print("Fibonacci sequence (iterative):")
  var i: Int = 0
  while i < 20 {
    print(fibIterative(i))
    i = i + 1
  }

  // Verify recursive matches iterative for small N
  print("Verifying recursive vs iterative:")
  var allMatch: Int = 1
  var j: Int = 0
  while j < 15 {
    if fibRecursive(j) != fibIterative(j) {
      allMatch = 0
    }
    j = j + 1
  }
  print(allMatch) // 1 = all match

  // Print using generator
  print("First 10 via generator:")
  var count: Int = 0
  for fib in fibGenerator() {
    if count >= 10 {
      break
    }
    print(fib)
    count = count + 1
  }

  // Check specific Fibonacci numbers
  print("Is 13 Fibonacci?")
  if isFibonacci(13) {
    print(1)
  } else {
    print(0)
  }

  print("Is 14 Fibonacci?")
  if isFibonacci(14) {
    print(1)
  } else {
    print(0)
  }

  // Sum of first 10 Fibonacci numbers
  print("Sum of first 10:")
  print(fibSum(10))

  return 0
}
