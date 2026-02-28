// TinySwift End-to-End Compiler Test Suite
// Tests all syntax patterns against the full compilation pipeline.
// Build: tinyswift compile --no-prelude-import examples/compiler_test_suite.swift
// Run:   ./compiler_test_suite
//
// Every test prints a test ID followed by expected values.
// All values have been verified to produce correct output.

// ============================================================
// SECTION 1: Basic Types and Arithmetic
// ============================================================

func testIntArithmetic() {
  print("T01")
  print(2 + 3)       // 5
  print(10 - 7)      // 3
  print(4 * 5)       // 20
  print(20 / 4)      // 5
  print(17 % 5)      // 2
  print(0 - 42)      // -42
  return
}

func testIntComparisons() {
  print("T02")
  // Using helper since we can't print bool directly as true/false
  print(boolInt(3 < 5))    // 1
  print(boolInt(5 < 3))    // 0
  print(boolInt(3 <= 3))   // 1
  print(boolInt(3 >= 4))   // 0
  print(boolInt(5 == 5))   // 1
  print(boolInt(5 != 6))   // 1
  return
}

func boolInt(_ b: Bool) -> Int {
  if b { return 1 }
  return 0
}

func testBoolOperators() {
  print("T03")
  print(boolInt(true && true))    // 1
  print(boolInt(true && false))   // 0
  print(boolInt(false || true))   // 1
  print(boolInt(false || false))  // 0
  return
}

func testStringPrint() {
  print("T04")
  print("hello")
  print("spaces in between")
  print("")
  return
}

// ============================================================
// SECTION 2: Variables and Assignment
// ============================================================

func testVarMutation() {
  print("T05")
  var x: Int = 10
  print(x)         // 10
  x = 20
  print(x)         // 20
  x = x + 5
  print(x)         // 25
  return
}

func testLetAssignment() {
  print("T06")
  let a: Int = 42
  let b: Int = a + 8
  let c: Int = b * 2
  print(a)  // 42
  print(b)  // 50
  print(c)  // 100
  return
}

func testTypeInference() {
  print("T07")
  let x = 42
  let s = "hello"
  print(x)  // 42
  print(s)  // hello
  return
}

// ============================================================
// SECTION 3: Functions
// ============================================================

func add3(_ a: Int, _ b: Int, _ c: Int) -> Int {
  return a + b + c
}

func testMultipleParams() {
  print("T08")
  print(add3(1, 2, 3))  // 6
  return
}

func testInout() {
  print("T09")
  var val: Int = 10
  increment(&val)
  increment(&val)
  increment(&val)
  print(val)  // 13
  return
}

func increment(_ x: inout Int) {
  x = x + 1
  return
}

func testNestedFunction() {
  print("T10")
  func inner(_ y: Int) -> Int {
    return y * 2
  }
  print(inner(21))  // 42
  return
}

func testRecursion() {
  print("T11")
  print(factorial(5))   // 120
  print(fib(10))        // 55
  return
}

func factorial(_ n: Int) -> Int {
  if n <= 1 { return 1 }
  return n * factorial(n - 1)
}

func fib(_ n: Int) -> Int {
  if n <= 0 { return 0 }
  if n == 1 { return 1 }
  return fib(n - 1) + fib(n - 2)
}

func testMutualRecursion() {
  print("T12")
  print(boolInt(isEvenRec(10)))  // 1
  print(boolInt(isOddRec(7)))    // 1
  return
}

func isEvenRec(_ n: Int) -> Bool {
  if n == 0 { return true }
  return isOddRec(n - 1)
}

func isOddRec(_ n: Int) -> Bool {
  if n == 0 { return false }
  return isEvenRec(n - 1)
}

func testDeepCallChain() {
  print("T13")
  print(chainE(1))   // 23
  return
}

func chainA(_ x: Int) -> Int { return x + 1 }
func chainB(_ x: Int) -> Int { return chainA(x) * 2 }
func chainC(_ x: Int) -> Int { return chainB(x) + 3 }
func chainD(_ x: Int) -> Int { return chainC(x) * 4 }
func chainE(_ x: Int) -> Int { return chainD(x) - 5 }

func testLabeledArgs() {
  print("T14")
  print(greet(name: "world"))  // Hello world
  return
}

func greet(name: String) -> String {
  return "Hello " + name
}

// ============================================================
// SECTION 4: Control Flow
// ============================================================

func testIfReturn() {
  print("T15")
  // if-body with return works (false path falls through)
  print(classify(100))   // big
  print(classify(5))     // small
  return
}

func classify(_ x: Int) -> String {
  if x > 50 {
    return "big"
  }
  return "small"
}

func testIfElseBothReturn() {
  print("T16")
  // if-else where BOTH branches return works
  print(sign(5))     // 1
  print(sign(0 - 3)) // -1
  return
}

func sign(_ x: Int) -> Int {
  if x > 0 {
    return 1
  } else {
    return 0 - 1
  }
}

func testSequentialIfs() {
  print("T17")
  // Sequential if-return works (NOT else-if)
  print(grade(95))   // 10
  print(grade(85))   // 20
  print(grade(75))   // 30
  print(grade(50))   // 0
  return
}

func grade(_ score: Int) -> Int {
  if score >= 90 { return 10 }
  if score >= 80 { return 20 }
  if score >= 70 { return 30 }
  return 0
}

func testWhileLoop() {
  print("T18")
  print(sumTo(10))   // 55
  print(sumTo(100))  // 5050
  return
}

func sumTo(_ n: Int) -> Int {
  var total: Int = 0
  var i: Int = 1
  while i <= n {
    total = total + i
    i = i + 1
  }
  return total
}

func testWhileBreak() {
  print("T19")
  var i: Int = 0
  while true {
    i = i + 1
    if i == 5 { break }
  }
  print(i)  // 5
  return
}

func testWhileContinue() {
  print("T20")
  // continue works in while
  var i: Int = 0
  var sum: Int = 0
  while i < 10 {
    i = i + 1
    if i % 2 == 0 { continue }
    sum = sum + i
  }
  print(sum)  // 25 (1+3+5+7+9)
  return
}

func testNestedWhile() {
  print("T21")
  var total: Int = 0
  var i: Int = 1
  while i <= 3 {
    var j: Int = 1
    while j <= 3 {
      total = total + i * j
      j = j + 1
    }
    i = i + 1
  }
  print(total)  // 36
  return
}

func testForIn() {
  print("T22")
  var sum: Int = 0
  for i in 1...10 {
    sum = sum + i
  }
  print(sum)  // 55
  return
}

func testForInArray() {
  print("T23")
  let items: [Int] = [10, 20, 30]
  for item in items {
    print(item)
  }
  // 10 20 30
  return
}

func testForInStringArray() {
  print("T24")
  let names: [String] = ["a", "b", "c"]
  for name in names {
    print(name)
  }
  // a b c
  return
}

// ============================================================
// SECTION 5: Switch
// ============================================================

func testSwitchInt() {
  print("T25")
  print(dayType(1))   // one
  print(dayType(3))   // three
  print(dayType(99))  // other
  return
}

func dayType(_ x: Int) -> String {
  switch x {
  case 1: return "one"
  case 2: return "two"
  case 3: return "three"
  default: return "other"
  }
}

func testSwitchString() {
  print("T26")
  print(cmdToInt("add"))      // 1
  print(cmdToInt("mul"))      // 3
  print(cmdToInt("unknown"))  // 0
  return
}

func cmdToInt(_ cmd: String) -> Int {
  switch cmd {
  case "add": return 1
  case "sub": return 2
  case "mul": return 3
  default: return 0
  }
}

func testSwitchManyCase() {
  print("T27")
  print(manyCase(1))   // 10
  print(manyCase(3))   // 30
  print(manyCase(5))   // 50
  print(manyCase(99))  // 0
  return
}

func manyCase(_ x: Int) -> Int {
  switch x {
  case 1: return 10
  case 2: return 20
  case 3: return 30
  case 4: return 40
  case 5: return 50
  default: return 0
  }
}

// ============================================================
// SECTION 6: Structs
// ============================================================

struct Point {
  var x: Int
  var y: Int
}

func testStructBasic() {
  print("T28")
  let p: Point = Point(x: 3, y: 4)
  print(p.x)         // 3
  print(p.y)         // 4
  print(p.x + p.y)   // 7
  return
}

struct Rect {
  var width: Int
  var height: Int
  func area() -> Int {
    return self.width * self.height
  }
  func perimeter() -> Int {
    return 2 * (self.width + self.height)
  }
}

func testStructMethod() {
  print("T29")
  let r: Rect = Rect(width: 5, height: 3)
  print(r.area())       // 15
  print(r.perimeter())  // 16
  return
}

struct Vec2 {
  var x: Int
  var y: Int
  func add(_ other: Vec2) -> Vec2 {
    return Vec2(x: self.x + other.x, y: self.y + other.y)
  }
  func dot(_ other: Vec2) -> Int {
    return self.x * other.x + self.y * other.y
  }
}

func testStructReturn() {
  print("T30")
  let a: Vec2 = Vec2(x: 1, y: 2)
  let b: Vec2 = Vec2(x: 3, y: 4)
  let c: Vec2 = a.add(b)
  print(c.x)       // 4
  print(c.y)       // 6
  print(a.dot(b))  // 11
  return
}

struct Builder {
  var value: Int
  func add(_ n: Int) -> Builder {
    return Builder(value: self.value + n)
  }
  func multiply(_ n: Int) -> Builder {
    return Builder(value: self.value * n)
  }
}

func testStructChain() {
  print("T31")
  let result: Builder = Builder(value: 0).add(5).multiply(3).add(7)
  print(result.value)  // 22
  return
}

struct Named {
  var name: String
  var age: Int
}

func testStructString() {
  print("T32")
  let p: Named = Named(name: "Alice", age: 30)
  print(p.name)  // Alice
  print(p.age)   // 30
  return
}

func testStructCopy() {
  print("T33")
  let a: Point = Point(x: 1, y: 2)
  let b: Point = a
  // Value semantics: b is a copy
  print(a.x)  // 1
  print(b.x)  // 1
  return
}

struct Inner {
  var x: Int
}
struct Outer {
  var inner: Inner
  var y: Int
}

func testStructInStruct() {
  print("T34")
  let i: Inner = Inner(x: 10)
  let o: Outer = Outer(inner: i, y: 20)
  print(o.inner.x)  // 10
  print(o.y)        // 20
  return
}

struct A3 {
  var value: Int
}
struct B3 {
  var a: A3
}
struct C3 {
  var b: B3
}

func testDeepStruct() {
  print("T35")
  let c: C3 = C3(b: B3(a: A3(value: 42)))
  print(c.b.a.value)  // 42
  return
}

struct RecCounter {
  var n: Int
  func countDown() -> Int {
    if self.n <= 0 { return 0 }
    let next: RecCounter = RecCounter(n: self.n - 1)
    return self.n + next.countDown()
  }
}

func testStructRecursion() {
  print("T36")
  let c: RecCounter = RecCounter(n: 5)
  print(c.countDown())  // 15
  return
}

// ============================================================
// SECTION 7: Enums
// ============================================================

enum Color {
  case red
  case green
  case blue
}

func colorName(_ c: Color) -> String {
  switch c {
  case .red: return "red"
  case .green: return "green"
  case .blue: return "blue"
  }
}

func testEnumBasic() {
  print("T37")
  print(colorName(Color.red))    // red
  print(colorName(Color.green))  // green
  print(colorName(Color.blue))   // blue
  return
}

enum Shape {
  case circle(Int)
  case square(Int)
  case rect(Int)
}

func shapeVal(_ s: Shape) -> Int {
  switch s {
  case .circle(let r): return r
  case .square(let side): return side * side
  case .rect(let area): return area
  }
}

func testEnumAssocValue() {
  print("T38")
  print(shapeVal(Shape.circle(5)))   // 5
  print(shapeVal(Shape.square(4)))   // 16
  print(shapeVal(Shape.rect(42)))    // 42
  return
}

enum Direction {
  case north
  case south
  case east
  case west
  func opposite() -> Direction {
    switch self {
    case .north: return Direction.south
    case .south: return Direction.north
    case .east: return Direction.west
    case .west: return Direction.east
    }
  }
}

func dirName(_ d: Direction) -> String {
  switch d {
  case .north: return "N"
  case .south: return "S"
  case .east: return "E"
  case .west: return "W"
  }
}

func testEnumMethod() {
  print("T39")
  print(dirName(Direction.north.opposite()))  // S
  print(dirName(Direction.east.opposite()))   // W
  return
}

enum Coin {
  case penny
  case nickel
  case dime
  case quarter
  func value() -> Int {
    switch self {
    case .penny: return 1
    case .nickel: return 5
    case .dime: return 10
    case .quarter: return 25
    }
  }
}

func testEnumIntMethod() {
  print("T40")
  print(Coin.penny.value())    // 1
  print(Coin.quarter.value())  // 25
  print(Coin.penny.value() + Coin.quarter.value())  // 26
  return
}

enum Season {
  case spring
  case summer
  case autumn
  case winter
}

func testEnumDefault() {
  print("T41")
  print(boolInt(isWarm(Season.summer)))  // 1
  print(boolInt(isWarm(Season.winter)))  // 0
  return
}

func isWarm(_ s: Season) -> Bool {
  switch s {
  case .summer: return true
  default: return false
  }
}

enum Tag {
  case label(String)
  case empty
}

func describeTag(_ t: Tag) -> String {
  switch t {
  case .label(let s): return s
  case .empty: return "none"
  }
}

func testEnumStringAssoc() {
  print("T42")
  print(describeTag(Tag.label("hello")))  // hello
  print(describeTag(Tag.empty))           // none
  return
}

// ============================================================
// SECTION 8: Strings
// ============================================================

func testStringConcat() {
  print("T43")
  let result: String = "one" + " " + "two" + " " + "three"
  print(result)  // one two three
  return
}

func testStringFunc() {
  print("T44")
  print(repeat2("ab"))  // abab
  return
}

func repeat2(_ s: String) -> String {
  return s + s
}

func testStringCount() {
  print("T45")
  let s: String = "hello"
  print(s.count)  // 5
  return
}

func testStringInterp() {
  print("T46")
  let x: Int = 42
  print("x = \(x)")  // x = 42
  let name: String = "world"
  print("hello \(name)")  // hello world
  return
}

// ============================================================
// SECTION 9: Optionals
// ============================================================

func testOptionalForceUnwrap() {
  print("T47")
  let a: Int? = 42
  print(a!)  // 42
  return
}

func testOptionalReturn() {
  print("T48")
  let a: Int? = findPositive(42)
  print(a!)  // 42
  let b: Int? = findPositive(0 - 5)
  let fallback: Int = b ?? 0
  print(fallback)  // 0
  return
}

func findPositive(_ x: Int) -> Int? {
  if x > 0 { return x }
  return nil
}

func testOptionalChaining() {
  print("T49")
  let p: Named? = Named(name: "Bob", age: 25)
  let age: Int? = p?.age
  print(age!)  // 25
  return
}

func testNilCoalescing() {
  print("T50")
  let a: Int? = 42
  let b: Int? = nil
  print(a ?? 0)   // 42
  print(b ?? 99)  // 99
  return
}

// ============================================================
// SECTION 10: Extensions, Defer, Misc
// ============================================================

struct Circle {
  var radius: Int
}

extension Circle {
  func approxArea() -> Int {
    return 3 * self.radius * self.radius
  }
}

func testExtension() {
  print("T51")
  let c: Circle = Circle(radius: 5)
  print(c.approxArea())  // 75
  return
}

func testDefer() {
  print("T52")
  deferDemo()
  return
}

func deferDemo() {
  defer { print(99) }
  print(1)
  return
}

func testTypealias() {
  print("T53")
  typealias Coord = Int
  let x: Coord = 42
  print(x)  // 42
  return
}

func testBitwise() {
  print("T54")
  print(5 & 3)   // 1
  print(5 | 3)   // 7
  print(5 ^ 3)   // 6
  print(1 << 4)  // 16
  print(16 >> 2) // 4
  return
}

func testVoidFunction() {
  print("T55")
  printTwo(1, 2)
  return
}

func printTwo(_ a: Int, _ b: Int) {
  print(a)
  print(b)
  return
}

func testComplexBool() {
  print("T56")
  let a: Int = 5
  let b: Int = 10
  print(boolInt(a > 0 && b > 0))                // 1
  print(boolInt(a > 0 && b < 0))                // 0
  print(boolInt(a < 0 || b > 0))                // 1
  print(boolInt(a > 3 && b < 20 && a + b > 10)) // 1
  return
}

// ============================================================
// ENTRY POINT
// ============================================================

func main() -> Int {
  testIntArithmetic()
  testIntComparisons()
  testBoolOperators()
  testStringPrint()
  testVarMutation()
  testLetAssignment()
  testTypeInference()
  testMultipleParams()
  testInout()
  testNestedFunction()
  testRecursion()
  testMutualRecursion()
  testDeepCallChain()
  testLabeledArgs()
  testIfReturn()
  testIfElseBothReturn()
  testSequentialIfs()
  testWhileLoop()
  testWhileBreak()
  testWhileContinue()
  testNestedWhile()
  testForIn()
  testForInArray()
  testForInStringArray()
  testSwitchInt()
  testSwitchString()
  testSwitchManyCase()
  testStructBasic()
  testStructMethod()
  testStructReturn()
  testStructChain()
  testStructString()
  testStructCopy()
  testStructInStruct()
  testDeepStruct()
  testStructRecursion()
  testEnumBasic()
  testEnumAssocValue()
  testEnumMethod()
  testEnumIntMethod()
  testEnumDefault()
  testEnumStringAssoc()
  testStringConcat()
  testStringFunc()
  testStringCount()
  testStringInterp()
  testOptionalForceUnwrap()
  testOptionalReturn()
  testOptionalChaining()
  testNilCoalescing()
  testExtension()
  testDefer()
  testTypealias()
  testBitwise()
  testVoidFunction()
  testComplexBool()
  return 0
}
