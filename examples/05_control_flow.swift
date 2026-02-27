// TinySwift Example: Control Flow
// Demonstrates if/else, guard, while, repeat-while, for-in, break, continue.

// --- If / else ---

func classify(_ n: Int) -> String {
  if n > 0 {
    return "positive"
  } else if n < 0 {
    return "negative"
  } else {
    return "zero"
  }
}

// --- Nested if ---

func fizzBuzz(_ n: Int) -> String {
  if n % 15 == 0 {
    return "FizzBuzz"
  } else if n % 3 == 0 {
    return "Fizz"
  } else if n % 5 == 0 {
    return "Buzz"
  }
  return ""
}

// --- Guard statement (early exit) ---

func processPositive(_ n: Int) -> Int {
  guard n > 0 else {
    return 0
  }
  return n * 2
}

func requireNonEmpty(_ s: String) -> String {
  guard s != "" else {
    return "default"
  }
  return s
}

// --- While loop ---

func sumUpTo(_ n: Int) -> Int {
  var total: Int = 0
  var i: Int = 1
  while i <= n {
    total = total + i
    i = i + 1
  }
  return total
}

// --- While with break ---

func findFirstDivisible(_ target: Int, by divisor: Int) -> Int {
  var n: Int = 1
  while true {
    if n % divisor == 0 {
      break
    }
    n = n + 1
  }
  return n
}

// --- Repeat-while loop (do-while equivalent) ---

func countDigits(_ number: Int) -> Int {
  var n: Int = number
  var digits: Int = 0
  repeat {
    n = n / 10
    digits = digits + 1
  } while n > 0
  return digits
}

// --- For-in loop ---

func sumArray(_ items: [Int]) -> Int {
  var total: Int = 0
  for item in items {
    total = total + item
  }
  return total
}

// --- For-in with continue ---

func sumEvenOnly(_ items: [Int]) -> Int {
  var total: Int = 0
  for item in items {
    if item % 2 != 0 {
      continue
    }
    total = total + item
  }
  return total
}

// --- For-in with break ---

func findFirst(_ items: [Int], greaterThan threshold: Int) -> Int {
  var result: Int = -1
  for item in items {
    if item > threshold {
      result = item
      break
    }
  }
  return result
}

// --- Nested loops ---

func multiplicationTable(_ size: Int) -> Int {
  var count: Int = 0
  var i: Int = 1
  while i <= size {
    var j: Int = 1
    while j <= size {
      count = count + 1
      j = j + 1
    }
    i = i + 1
  }
  return count
}
