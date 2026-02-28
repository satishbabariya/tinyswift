// TinySwift Algorithmic Example: Number Theory
// Demonstrates: math algorithms, while loops, recursion, boolean logic.
// Build: tinyswift compile examples/number_theory.swift
// Run:   ./number_theory

// --- Primality test ---

func isPrime(_ n: Int) -> Bool {
  if n < 2 {
    return false
  }
  if n == 2 {
    return true
  }
  if n % 2 == 0 {
    return false
  }
  var i: Int = 3
  while i * i <= n {
    if n % i == 0 {
      return false
    }
    i = i + 2
  }
  return true
}

// --- Sieve of Eratosthenes (count primes up to n) ---

func countPrimesUpTo(_ n: Int) -> Int {
  var count: Int = 0
  var i: Int = 2
  while i <= n {
    if isPrime(i) {
      count = count + 1
    }
    i = i + 1
  }
  return count
}

// --- Nth prime ---

func nthPrime(_ n: Int) -> Int {
  var count: Int = 0
  var candidate: Int = 2
  while true {
    if isPrime(candidate) {
      count = count + 1
      if count == n {
        return candidate
      }
    }
    candidate = candidate + 1
  }
}

// --- Prime factorization (print factors) ---

func printPrimeFactors(_ n: Int) -> Void {
  var num: Int = n
  var divisor: Int = 2
  while divisor * divisor <= num {
    while num % divisor == 0 {
      print(divisor)
      num = num / divisor
    }
    divisor = divisor + 1
  }
  if num > 1 {
    print(num)
  }
}

// --- GCD (Euclidean) ---

func gcd(_ a: Int, _ b: Int) -> Int {
  var x: Int = a
  var y: Int = b
  if x < 0 { x = 0 - x }
  if y < 0 { y = 0 - y }
  while y != 0 {
    let temp: Int = y
    y = x % y
    x = temp
  }
  return x
}

// --- LCM ---

func lcm(_ a: Int, _ b: Int) -> Int {
  if a == 0 || b == 0 {
    return 0
  }
  let g: Int = gcd(a, b)
  return (a / g) * b
}

// --- Modular exponentiation: (base^exp) mod m ---

func modPow(_ base: Int, _ exp: Int, _ m: Int) -> Int {
  var result: Int = 1
  var b: Int = base % m
  var e: Int = exp
  while e > 0 {
    if e % 2 == 1 {
      result = (result * b) % m
    }
    e = e / 2
    b = (b * b) % m
  }
  return result
}

// --- Fibonacci (modular) ---

func fibMod(_ n: Int, _ m: Int) -> Int {
  if n <= 0 {
    return 0
  }
  if n == 1 {
    return 1 % m
  }
  var a: Int = 0
  var b: Int = 1
  var i: Int = 2
  while i <= n {
    let temp: Int = (a + b) % m
    a = b
    b = temp
    i = i + 1
  }
  return b
}

// --- Sum of divisors ---

func sumOfDivisors(_ n: Int) -> Int {
  var total: Int = 0
  var i: Int = 1
  while i * i <= n {
    if n % i == 0 {
      total = total + i
      if i != n / i {
        total = total + n / i
      }
    }
    i = i + 1
  }
  return total
}

// --- Check if perfect number ---

func isPerfect(_ n: Int) -> Bool {
  if n < 2 {
    return false
  }
  return sumOfDivisors(n) - n == n
}

// --- Collatz conjecture: count steps to reach 1 ---

func collatzSteps(_ n: Int) -> Int {
  var num: Int = n
  var steps: Int = 0
  while num != 1 {
    if num % 2 == 0 {
      num = num / 2
    } else {
      num = 3 * num + 1
    }
    steps = steps + 1
  }
  return steps
}

// --- Count divisors ---

func countDivisors(_ n: Int) -> Int {
  var count: Int = 0
  var i: Int = 1
  while i * i <= n {
    if n % i == 0 {
      count = count + 1
      if i != n / i {
        count = count + 1
      }
    }
    i = i + 1
  }
  return count
}

// --- Entry point ---

func main() -> Int {
  // Primality tests
  print("Prime checks:")
  var i: Int = 2
  while i <= 30 {
    if isPrime(i) {
      print(i)
    }
    i = i + 1
  }

  // Count primes
  print("Primes up to 100:")
  print(countPrimesUpTo(100))  // 25

  // Nth prime
  print("10th prime:")
  print(nthPrime(10))  // 29

  // Prime factorization
  print("Factors of 360:")
  printPrimeFactors(360)  // 2 2 2 3 3 5

  // GCD / LCM
  print("GCD(252, 105):")
  print(gcd(252, 105))  // 21

  print("LCM(12, 15):")
  print(lcm(12, 15))  // 60

  // Modular exponentiation
  print("2^10 mod 1000:")
  print(modPow(2, 10, 1000))  // 24

  print("3^13 mod 100:")
  print(modPow(3, 13, 100))  // 23 (1594323 % 100 = 23)

  // Perfect numbers
  print("Perfect numbers up to 30:")
  var j: Int = 2
  while j <= 30 {
    if isPerfect(j) {
      print(j)
    }
    j = j + 1
  }
  // Should print 6 and 28

  // Collatz
  print("Collatz steps for 27:")
  print(collatzSteps(27))  // 111

  print("Collatz steps for 1:")
  print(collatzSteps(1))   // 0

  // Divisor count
  print("Divisors of 36:")
  print(countDivisors(36))  // 9

  print("Divisors of 100:")
  print(countDivisors(100))  // 9

  // Sum of divisors
  print("Sum of divisors of 28:")
  print(sumOfDivisors(28))  // 56 (1+2+4+7+14+28)

  // Fibonacci modular
  print("fib(50) mod 1000000007:")
  print(fibMod(50, 1000000007))  // 12586269025 mod 1e9+7

  return 0
}
