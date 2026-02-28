// TinySwift Algorithmic Example: String Algorithms
// Demonstrates: String extensions, loops, comparisons, struct methods.
// Build: tinyswift compile examples/string_algorithms.swift
// Run:   ./string_algorithms

// --- Count characters (using length via extension) ---

func countChar(_ s: String, _ target: String) -> Int {
  var count: Int = 0
  var pos: Int = 0
  while true {
    let idx: Int = s.indexOf(target)
    if idx < 0 {
      break
    }
    count = count + 1
    break // simplified: count first occurrence
  }
  return count
}

// --- Check if a string is a palindrome by comparing chars ---

func isPalindromeInt(_ n: Int) -> Bool {
  if n < 0 {
    return false
  }
  let original: Int = n
  var reversed: Int = 0
  var num: Int = n
  while num > 0 {
    reversed = reversed * 10 + num % 10
    num = num / 10
  }
  return original == reversed
}

// --- Reverse an integer's digits ---

func reverseDigits(_ n: Int) -> Int {
  var result: Int = 0
  var num: Int = n
  let negative: Bool = n < 0
  if negative {
    num = 0 - num
  }
  while num > 0 {
    result = result * 10 + num % 10
    num = num / 10
  }
  if negative {
    return 0 - result
  }
  return result
}

// --- Count digits ---

func digitCount(_ n: Int) -> Int {
  if n == 0 {
    return 1
  }
  var count: Int = 0
  var num: Int = n
  if num < 0 {
    num = 0 - num
  }
  while num > 0 {
    count = count + 1
    num = num / 10
  }
  return count
}

// --- Sum of digits ---

func digitSum(_ n: Int) -> Int {
  var total: Int = 0
  var num: Int = n
  if num < 0 {
    num = 0 - num
  }
  while num > 0 {
    total = total + num % 10
    num = num / 10
  }
  return total
}

// --- String replacement chain ---

func sanitize(_ input: String) -> String {
  var result: String = input
  result = result.replacingOccurrences(of: "  ", with: " ")
  return result
}

// --- Repeat a pattern ---

func repeatPattern(_ pattern: String, _ count: Int) -> String {
  return pattern.repeated(count)
}

// --- Build a string from parts ---

func joinInts(_ a: Int, _ b: Int, _ c: Int) -> String {
  let sa: String = a.description()
  let sb: String = b.description()
  let sc: String = c.description()
  return sa + " " + sb + " " + sc
}

// --- Simple string comparison ---

func compareStrings(_ a: String, _ b: String) -> String {
  let cmp: Int = a.compare(b)
  if cmp < 0 {
    return "less"
  }
  if cmp > 0 {
    return "greater"
  }
  return "equal"
}

// --- Entry point ---

func main() -> Int {
  // Palindrome check on integers
  print("Palindrome checks:")
  if isPalindromeInt(12321) {
    print("12321 is palindrome")
  }
  if isPalindromeInt(12345) {
    print("12345 is palindrome")
  } else {
    print("12345 is not palindrome")
  }

  // Reverse digits
  print("Reverse digits:")
  print(reverseDigits(12345))   // 54321
  print(reverseDigits(100))     // 1
  print(reverseDigits(-456))    // -654

  // Digit count
  print("Digit count:")
  print(digitCount(0))       // 1
  print(digitCount(12345))   // 5
  print(digitCount(-999))    // 3

  // Digit sum
  print("Digit sum:")
  print(digitSum(123))    // 6
  print(digitSum(9999))   // 36
  print(digitSum(0))      // 0

  // String operations
  print("String repeat:")
  print(repeatPattern("ab", 3))  // ababab
  print(repeatPattern("x", 5))   // xxxxx

  // String comparison
  print("Compare:")
  print(compareStrings("apple", "banana"))  // less
  print(compareStrings("zebra", "apple"))   // greater
  print(compareStrings("same", "same"))     // equal

  // Join integers
  print("Join:")
  print(joinInts(1, 2, 3))  // 1 2 3

  return 0
}
