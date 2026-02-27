// TinySwift Example: Operators
// Demonstrates arithmetic, comparison, logical, and other operators.

// --- Arithmetic operators ---

let sum: Int = 10 + 3         // 13
let difference: Int = 10 - 3  // 7
let product: Int = 10 * 3     // 30
let quotient: Int = 10 / 3    // 3
let remainder: Int = 10 % 3   // 1

let dSum: Double = 1.5 + 2.5    // 4.0
let dDiff: Double = 5.0 - 1.2   // 3.8
let dProd: Double = 2.0 * 3.5   // 7.0
let dQuot: Double = 10.0 / 3.0  // 3.333...

// --- Comparison operators ---

let isEqual: Bool = 5 == 5      // true
let isNotEqual: Bool = 5 != 3   // true
let isLess: Bool = 3 < 5        // true
let isGreater: Bool = 5 > 3     // true
let isLessEq: Bool = 5 <= 5     // true
let isGreaterEq: Bool = 5 >= 3  // true

// --- Logical operators ---

let andResult: Bool = true && false  // false
let orResult: Bool = true || false   // true
let notResult: Bool = !true          // false

let compound: Bool = (5 > 3) && (2 < 4)  // true

// --- Ternary conditional operator ---

let a: Int = 10
let b: Int = 20
let bigger: Int = a > b ? a : b  // 20

let label: String = a > 0 ? "positive" : "non-positive"

// --- Nil coalescing operator ---

let maybe: Int? = nil
let value: Int = maybe ?? 0  // 0

let present: String? = "hello"
let text: String = present ?? "default"  // "hello"

// --- Assignment operator ---

var count: Int = 0
count = count + 1
count = count * 2

// --- Bitwise operators ---

let shifted: Int = 1 << 4   // 16
let back: Int = 16 >> 2     // 4
let bitwiseOr: Int = 5 | 3  // 7
let bitwiseAnd: Int = 5 & 3 // 1
let bitwiseXor: Int = 5 ^ 3 // 6
let bitwiseNot: Int = ~0    // -1

// --- Range operator ---

let range = 0...10  // closed range
