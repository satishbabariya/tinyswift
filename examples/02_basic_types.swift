// TinySwift Example: Basic Types
// Demonstrates the four primitive types: Int, Double, Bool, String.

// --- Int ---

let zero: Int = 0
let positive: Int = 42
let negative: Int = -17
let large: Int = 1000000

// --- Double ---

let wholeDouble: Double = 1.0
let fraction: Double = 3.14
let scientific: Double = 2.71828182845904523536
let tiny: Double = 0.001

// --- Bool ---

let yes: Bool = true
let no: Bool = false

// --- String ---

let empty: String = ""
let hello: String = "Hello, world!"
let multiWord: String = "TinySwift is a modern language"

// --- Optional types ---

let maybeInt: Int? = nil
let someInt: Int? = 42
let maybeString: String? = nil
let someString: String? = "present"

// --- Implicitly unwrapped optionals ---

let definitelyInt: Int! = 100

// --- Array types ---

let numbers: [Int] = [1, 2, 3, 4, 5]
let names: [String] = ["Alice", "Bob", "Charlie"]
let empty_array: [Int] = []

// --- Dictionary types ---

let ages: [String: Int] = ["Alice": 30, "Bob": 25]
let scores: [Int: String] = [100: "perfect", 0: "zero"]

// --- Tuple types ---

let pair: (Int, String) = (42, "answer")
let triple: (Int, Double, Bool) = (1, 2.0, true)
let named: (x: Int, y: Int) = (x: 10, y: 20)

// --- Function types ---

let operation: (Int, Int) -> Int = add
let predicate: (Int) -> Bool = isPositive

func add(_ a: Int, _ b: Int) -> Int {
  return a + b
}

func isPositive(_ n: Int) -> Bool {
  return n > 0
}
