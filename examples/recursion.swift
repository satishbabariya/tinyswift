// TinySwift Algorithmic Example: Recursion
// Demonstrates: recursive functions, divide and conquer, mathematical recursion.
// Build: tinyswift compile examples/recursion.swift
// Run:   ./recursion

// --- Factorial ---

func factorial(_ n: Int) -> Int {
  if n <= 1 {
    return 1
  }
  return n * factorial(n - 1)
}

// --- Power (fast exponentiation) ---

func power(_ base: Int, _ exp: Int) -> Int {
  if exp == 0 {
    return 1
  }
  if exp == 1 {
    return base
  }
  let half: Int = power(base, exp / 2)
  if exp % 2 == 0 {
    return half * half
  }
  return half * half * base
}

// --- Sum of array (recursive) ---

func sumArray(_ arr: [Int], _ size: Int) -> Int {
  if size == 0 {
    return 0
  }
  return arr[size - 1] + sumArray(arr, size - 1)
}

// --- Max of array (recursive) ---

func maxArray(_ arr: [Int], _ size: Int) -> Int {
  if size == 1 {
    return arr[0]
  }
  let subMax: Int = maxArray(arr, size - 1)
  if arr[size - 1] > subMax {
    return arr[size - 1]
  }
  return subMax
}

// --- Tower of Hanoi (count moves) ---

func hanoiMoves(_ n: Int) -> Int {
  if n == 1 {
    return 1
  }
  return 2 * hanoiMoves(n - 1) + 1
}

// --- Binomial coefficient C(n, k) ---

func choose(_ n: Int, _ k: Int) -> Int {
  if k == 0 || k == n {
    return 1
  }
  return choose(n - 1, k - 1) + choose(n - 1, k)
}

// --- GCD (recursive Euclidean) ---

func gcd(_ a: Int, _ b: Int) -> Int {
  if b == 0 {
    return a
  }
  return gcd(b, a % b)
}

// --- Ackermann function ---

func ackermann(_ m: Int, _ n: Int) -> Int {
  if m == 0 {
    return n + 1
  }
  if n == 0 {
    return ackermann(m - 1, 1)
  }
  return ackermann(m - 1, ackermann(m, n - 1))
}

// --- Digital root (repeated digit sum) ---

func digitalRoot(_ n: Int) -> Int {
  if n < 10 {
    return n
  }
  var sum: Int = 0
  var num: Int = n
  while num > 0 {
    sum = sum + num % 10
    num = num / 10
  }
  return digitalRoot(sum)
}

// --- Count paths in a grid (top-left to bottom-right, only right/down) ---

func countPaths(_ rows: Int, _ cols: Int) -> Int {
  if rows == 1 || cols == 1 {
    return 1
  }
  return countPaths(rows - 1, cols) + countPaths(rows, cols - 1)
}

// --- Sum of digits ---

func sumDigits(_ n: Int) -> Int {
  if n < 10 {
    return n
  }
  return n % 10 + sumDigits(n / 10)
}

// --- Catalan numbers ---

func catalan(_ n: Int) -> Int {
  if n <= 1 {
    return 1
  }
  var result: Int = 0
  var i: Int = 0
  while i < n {
    result = result + catalan(i) * catalan(n - 1 - i)
    i = i + 1
  }
  return result
}

// --- Entry point ---

func main() -> Int {
  // Factorials
  print("Factorials:")
  print(factorial(0))   // 1
  print(factorial(1))   // 1
  print(factorial(5))   // 120
  print(factorial(10))  // 3628800

  // Fast exponentiation
  print("Powers:")
  print(power(2, 0))    // 1
  print(power(2, 10))   // 1024
  print(power(3, 7))    // 2187
  print(power(5, 4))    // 625

  // Array recursion
  let arr: [Int] = [3, 1, 4, 1, 5, 9, 2, 6]
  print("Sum of array:")
  print(sumArray(arr, 8))  // 31

  print("Max of array:")
  print(maxArray(arr, 8))  // 9

  // Tower of Hanoi
  print("Hanoi moves:")
  print(hanoiMoves(1))   // 1
  print(hanoiMoves(3))   // 7
  print(hanoiMoves(5))   // 31
  print(hanoiMoves(10))  // 1023

  // Binomial coefficients (Pascal's triangle row 5)
  print("C(5,k) for k=0..5:")
  var k: Int = 0
  while k <= 5 {
    print(choose(5, k))
    k = k + 1
  }
  // 1 5 10 10 5 1

  // GCD
  print("GCD:")
  print(gcd(48, 18))   // 6
  print(gcd(100, 75))  // 25
  print(gcd(17, 13))   // 1

  // Ackermann (small values only!)
  print("Ackermann:")
  print(ackermann(0, 0))  // 1
  print(ackermann(1, 1))  // 3
  print(ackermann(2, 2))  // 7
  print(ackermann(3, 3))  // 61

  // Digital root
  print("Digital root:")
  print(digitalRoot(493))   // 7 (4+9+3=16, 1+6=7)
  print(digitalRoot(9999))  // 9

  // Grid paths
  print("Grid paths (3x3):")
  print(countPaths(3, 3))  // 6

  print("Grid paths (4x4):")
  print(countPaths(4, 4))  // 20

  // Sum digits
  print("Sum digits:")
  print(sumDigits(12345))  // 15

  // Catalan numbers
  print("Catalan numbers:")
  print(catalan(0))  // 1
  print(catalan(1))  // 1
  print(catalan(4))  // 14
  print(catalan(5))  // 42

  return 0
}
