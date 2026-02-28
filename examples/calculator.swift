// TinySwift Algorithmic Example: Expression Calculator
// Demonstrates: enums with associated values, switch, functions, recursion.
// Build: tinyswift compile examples/calculator.swift
// Run:   ./calculator

// --- Arithmetic operation enum ---

enum Op {
  case add
  case subtract
  case multiply
  case divide
}

func safeDivideInt(_ a: Int, _ b: Int) -> Int {
  if b == 0 {
    return 0
  }
  return a / b
}

func applyOp(_ op: Op, _ a: Int, _ b: Int) -> Int {
  switch op {
  case .add: return a + b
  case .subtract: return a - b
  case .multiply: return a * b
  case .divide: return safeDivideInt(a, b)
  }
}

func opName(_ op: Op) -> String {
  switch op {
  case .add: return "+"
  case .subtract: return "-"
  case .multiply: return "*"
  case .divide: return "/"
  }
}

// --- Expression evaluation using direct function calls ---

func evalNumber(_ n: Int) -> Int {
  return n
}

func evalBinary(_ op: Op, _ a: Int, _ b: Int) -> Int {
  return applyOp(op, a, b)
}

// --- Result type for safe operations ---
// Uses Int for both cases to work around compiler limitation with
// mixed associated value types. 0 = success, 1 = error.

enum CalcResult {
  case ok(Int)
  case error(Int)
}

func safeDivide(_ a: Int, _ b: Int) -> CalcResult {
  if b == 0 {
    return CalcResult.error(1)
  }
  return CalcResult.ok(a / b)
}

func safeModulo(_ a: Int, _ b: Int) -> CalcResult {
  if b == 0 {
    return CalcResult.error(2)
  }
  return CalcResult.ok(a % b)
}

func printErrorCode(_ code: Int) {
  switch code {
  case 1: print("division by zero")
  case 2: print("modulo by zero")
  default: print("unknown error")
  }
  return
}

func printCalcResult(_ result: CalcResult) {
  switch result {
  case .ok(let value):
    print(value)
  case .error(let code):
    printErrorCode(code)
  }
  return
}

// --- Power function (exponentiation by squaring) ---

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

// --- Helper for integer absolute value ---

func intAbs(_ x: Int) -> Int {
  if x < 0 {
    return 0 - x
  }
  return x
}

// --- GCD using Euclidean algorithm ---

func gcd(_ a: Int, _ b: Int) -> Int {
  var x: Int = intAbs(a)
  var y: Int = intAbs(b)
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
  return intAbs(a) / gcd(a, b) * intAbs(b)
}

// --- Entry point ---

func main() -> Int {
  // Basic operations
  print("Basic arithmetic:")
  print(applyOp(Op.add, 15, 27))       // 42
  print(applyOp(Op.subtract, 100, 58)) // 42
  print(applyOp(Op.multiply, 6, 7))    // 42
  print(applyOp(Op.divide, 84, 2))     // 42

  // Expression evaluation
  print("Expression evaluation:")
  print(evalNumber(42))
  print(evalBinary(Op.multiply, 6, 7))
  print(evalBinary(Op.add, 20, 22))

  // Safe division
  print("Safe division:")
  printCalcResult(safeDivide(100, 4))  // 25
  printCalcResult(safeDivide(10, 0))   // division by zero

  // Safe modulo
  print("Safe modulo:")
  printCalcResult(safeModulo(17, 5))   // 2
  printCalcResult(safeModulo(10, 0))   // modulo by zero

  // Power
  print("Powers:")
  print(power(2, 0))   // 1
  print(power(2, 1))   // 2
  print(power(2, 10))  // 1024
  print(power(3, 5))   // 243

  // GCD and LCM
  print("GCD(48, 18):")
  print(gcd(48, 18))  // 6

  print("GCD(100, 75):")
  print(gcd(100, 75)) // 25

  print("LCM(12, 18):")
  print(lcm(12, 18))  // 36

  print("LCM(4, 6):")
  print(lcm(4, 6))    // 12

  return 0
}
