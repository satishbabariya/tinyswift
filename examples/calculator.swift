// TinySwift Algorithmic Example: Expression Calculator
// Demonstrates: enums with associated values, switch, structs, closures, generics.
// Build: tinyswift compile examples/calculator.swift
// Run:   ./calculator

// --- Arithmetic operation enum ---

enum Op {
  case add
  case subtract
  case multiply
  case divide
}

func applyOp(_ op: Op, _ a: Int, _ b: Int) -> Int {
  switch op {
  case .add: return a + b
  case .subtract: return a - b
  case .multiply: return a * b
  case .divide:
    if b == 0 {
      return 0
    }
    return a / b
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

// --- Expression tree using enum ---

enum Expr {
  case number(Int)
  case binary(Op, Int, Int)
}

func evaluate(_ expr: Expr) -> Int {
  switch expr {
  case .number(let n):
    return n
  case .binary(let op, let a, let b):
    return applyOp(op, a, b)
  }
}

// --- Result type for safe operations ---

enum CalcResult {
  case ok(Int)
  case error(String)
}

func safeDivide(_ a: Int, _ b: Int) -> CalcResult {
  if b == 0 {
    return .error("division by zero")
  }
  return .ok(a / b)
}

func safeModulo(_ a: Int, _ b: Int) -> CalcResult {
  if b == 0 {
    return .error("modulo by zero")
  }
  return .ok(a % b)
}

func printResult(_ result: CalcResult) -> Void {
  switch result {
  case .ok(let value):
    print(value)
  case .error(let msg):
    print(msg)
  }
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

// --- GCD using Euclidean algorithm (uses prelude's abs()) ---

func gcd(_ a: Int, _ b: Int) -> Int {
  var x: Int = abs(a)
  var y: Int = abs(b)
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
  return abs(a) / gcd(a, b) * abs(b)
}

// --- Entry point ---

func main() -> Int {
  // Basic operations
  print("Basic arithmetic:")
  print(applyOp(.add, 15, 27))       // 42
  print(applyOp(.subtract, 100, 58)) // 42
  print(applyOp(.multiply, 6, 7))    // 42
  print(applyOp(.divide, 84, 2))     // 42

  // Expression evaluation
  print("Expression evaluation:")
  let e1: Expr = .number(42)
  print(evaluate(e1))

  let e2: Expr = .binary(.multiply, 6, 7)
  print(evaluate(e2))

  let e3: Expr = .binary(.add, 20, 22)
  print(evaluate(e3))

  // Safe division
  print("Safe division:")
  printResult(safeDivide(100, 4))  // 25
  printResult(safeDivide(10, 0))   // division by zero

  // Safe modulo
  print("Safe modulo:")
  printResult(safeModulo(17, 5))   // 2
  printResult(safeModulo(10, 0))   // modulo by zero

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
