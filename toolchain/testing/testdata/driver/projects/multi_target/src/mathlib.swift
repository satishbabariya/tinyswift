// A simple math library for integration testing.

func add(_ a: Int, _ b: Int) -> Int {
  return a + b
}

func multiply(_ a: Int, _ b: Int) -> Int {
  return a * b
}

func factorial(_ n: Int) -> Int {
  if n <= 1 {
    return 1
  }
  return n * factorial(n - 1)
}
