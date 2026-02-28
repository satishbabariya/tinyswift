// TinySwift Compiler Bug Catalog
// Documents all known compiler bugs found through systematic testing.
// Each bug includes a minimal reproduction case and the observed behavior.
//
// These are NOT meant to be compiled as a single file - each section is
// a standalone reproduction case.
//
// Bugs are categorized by severity:
//   [CRITICAL]  - Produces wrong results silently (no error, wrong output)
//   [CRASH-CG]  - Crashes during codegen (LLVM IR assertion)
//   [CRASH-RT]  - Compiles but crashes at runtime (segfault)
//   [CRASH-SIL] - Crashes during SIL generation/verification
//   [PARSE]     - Parser rejects valid syntax
//   [SEMANTIC]  - Semantic analysis rejects valid code
//   [MINOR]     - Incorrect but non-critical behavior

// ================================================================
// BUG 1: [CRITICAL] if-body without return falls off function
// ================================================================
// When an if-body does NOT contain a `return` statement, execution
// falls off the end of the function instead of continuing to the
// code after the if. This is the root cause of many downstream bugs.
//
// REPRODUCTION:
//   func main() -> Int {
//     var result: Int = 0
//     if true {
//       result = 42
//     }
//     print(result)  // NEVER REACHED
//     return 0
//   }
//
// EXPECTED: prints 42
// ACTUAL: no output, exits with garbage code
//
// IMPACT: Any program using if-without-return is broken.
// WORKAROUND: Ensure every if-body ends with `return`.

// ================================================================
// BUG 2: [CRITICAL] if-else body without return falls off function
// ================================================================
// Same as Bug 1 but for if-else. Both branches fall off.
//
// REPRODUCTION:
//   func main() -> Int {
//     var result: Int = 0
//     if 5 > 3 {
//       result = 42
//     } else {
//       result = 99
//     }
//     print(result)  // NEVER REACHED
//     return 0
//   }
//
// EXPECTED: prints 42
// ACTUAL: no output, exits with garbage code

// ================================================================
// BUG 3: [CRITICAL] if inside while - loop body falls off
// ================================================================
// When an if-body (without return) appears inside a while loop,
// the code after the if-body in the loop iteration never executes,
// and the loop doesn't continue.
//
// REPRODUCTION:
//   func main() -> Int {
//     var i: Int = 0
//     var sum: Int = 0
//     while i < 10 {
//       if i % 2 == 0 {
//         sum = sum + i   // Executes on first match, then falls off
//       }
//       i = i + 1         // NEVER REACHED after if-body executes
//     }
//     print(sum)           // Prints 0 (only first iter where 0%2==0, sum=0+0=0)
//     return 0
//   }
//
// EXPECTED: 20 (0+2+4+6+8)
// ACTUAL: 0 (falls off after first matching iteration)
//
// WORKAROUND: Move if-body logic into a separate function that
// returns a value, or restructure using if with early return.

// ================================================================
// BUG 4: [CRITICAL] Nested if with return - inner return lost
// ================================================================
// When an inner if-body has a return statement inside an outer
// if-body, the return is lost and execution falls through.
//
// REPRODUCTION:
//   func test(_ x: Int) -> Int {
//     if x > 0 {
//       if x > 100 {
//         return 999    // LOST - never returns this
//       }
//       return 10       // Falls here for ALL x > 0
//     }
//     return 0
//   }
//   // test(200) returns 10, expected 999

// ================================================================
// BUG 5: [CRITICAL] for-in with if - same as Bug 3
// ================================================================
// Same if-body fall-off bug in for-in loops.
//
// REPRODUCTION:
//   func main() -> Int {
//     var sum: Int = 0
//     for i in 1...10 {
//       if i % 2 == 0 {
//         sum = sum + i
//       }
//     }
//     print(sum)  // Prints 0 instead of 30
//     return 0
//   }

// ================================================================
// BUG 6: [CRITICAL] Struct field mutation silently ignored
// ================================================================
// Direct assignment to struct fields (`p.x = 10`) compiles but
// the value is never actually updated.
//
// REPRODUCTION:
//   struct Box { var value: Int }
//   func main() -> Int {
//     var b: Box = Box(value: 1)
//     b.value = 99
//     print(b.value)  // Prints 1, not 99
//     return 0
//   }
//
// WORKAROUND: Create a new struct instance instead of mutating:
//   b = Box(value: 99)

// ================================================================
// BUG 7: [CRITICAL] Tuple element access always returns first element
// ================================================================
// Accessing .1, .2, etc. on a tuple always returns the value of .0.
//
// REPRODUCTION:
//   func makePair(_ a: Int, _ b: Int) -> (Int, Int) {
//     return (a, b)
//   }
//   func main() -> Int {
//     let t: (Int, Int) = makePair(10, 20)
//     print(t.0)  // 10 (correct)
//     print(t.1)  // 10 (WRONG - should be 20)
//     return 0
//   }
//
// With 3 elements: t.0=10, t.1=10, t.2=10 (all same)

// ================================================================
// BUG 8: [CRITICAL] Switch range patterns never match
// ================================================================
// `case 1...5:` in a switch compiles but never matches any value.
// Always falls through to default.
//
// REPRODUCTION:
//   func test(_ x: Int) -> Int {
//     switch x {
//     case 1...5: return 1
//     case 6...10: return 2
//     default: return 0
//     }
//   }
//   // test(3) returns 0, expected 1
//   // test(7) returns 0, expected 2

// ================================================================
// BUG 9: [CRITICAL] Switch with multiple case values doesn't match
// ================================================================
// `case 1, 5, 9:` compiles but none of the values match.
//
// REPRODUCTION:
//   func test(_ c: Int) -> Bool {
//     switch c {
//     case 1, 5, 9: return true
//     default: return false
//     }
//   }
//   // test(1) returns false, expected true
//   // test(5) returns false, expected true

// ================================================================
// BUG 10: [CRASH-RT] else-if chains segfault
// ================================================================
// `if ... else if ... else` chains always segfault at runtime,
// even when all branches return.
//
// REPRODUCTION:
//   func test(_ x: Int) -> Int {
//     if x > 10 {
//       return 3
//     } else if x > 5 {
//       return 2
//     } else {
//       return 1
//     }
//   }
//
// Compiles successfully but segfaults when run.
// WORKAROUND: Use sequential if-return statements instead:
//   if x > 10 { return 3 }
//   if x > 5 { return 2 }
//   return 1

// ================================================================
// BUG 11: [CRASH-RT] Classes segfault at runtime
// ================================================================
// Any class instantiation segfaults. Struct equivalent works.
//
// REPRODUCTION:
//   class Box {
//     var value: Int
//     init(value: Int) { self.value = value }
//   }
//   func main() -> Int {
//     let b: Box = Box(value: 42)
//     print(b.value)  // SEGFAULT
//     return 0
//   }
//
// WORKAROUND: Use structs instead of classes.

// ================================================================
// BUG 12: [CRASH-RT] if-let segfaults
// ================================================================
// `if let val = optional { ... }` compiles but segfaults at runtime.
//
// REPRODUCTION:
//   func main() -> Int {
//     let a: Int? = 42
//     if let val = a {
//       print(val)  // SEGFAULT
//     }
//     return 0
//   }
//
// WORKAROUND: Use force unwrap (a!) or nil coalescing (a ?? default).

// ================================================================
// BUG 13: [CRASH-CG] Mixed-type enum associated values
// ================================================================
// Enum cases with different associated value types crash LLVM IR.
//
// REPRODUCTION:
//   enum Value {
//     case integer(Int)
//     case text(String)
//   }
//
// Error: "Initializer for struct element doesn't match!"
// WORKAROUND: Use same type for all associated values.

// ================================================================
// BUG 14: [CRASH-CG] Double operations crash codegen
// ================================================================
// Operations involving Double values crash with LLVM cast assertion.
// Double comparisons work, but arithmetic and print(Double) crash.
//
// REPRODUCTION:
//   func main() -> Int {
//     let x: Double = 3.14
//     print(x)  // CRASH: "Invalid cast!"
//     return 0
//   }
//
// Also crashes: Double + Double, Double(intValue)
// Works: Double comparison (3.14 > 2.0)

// ================================================================
// BUG 15: [CRASH-CG] Global variables crash codegen
// ================================================================
// Top-level var declarations crash with LLVM debug info error.
//
// REPRODUCTION:
//   var counter: Int = 0
//   func main() -> Int {
//     counter = counter + 1
//     print(counter)
//     return 0
//   }
//
// Error: "!dbg attachment points at wrong subprogram for function"

// ================================================================
// BUG 16: [CRASH-CG] Multiple enum types in one file
// ================================================================
// Having two or more enum type definitions in the same file crashes.
//
// REPRODUCTION:
//   enum A { case x; case y }
//   enum B { case m; case n }
//
// Note: This crash is intermittent and may depend on file complexity.
// The compiler_test_suite.swift has multiple enums and works, but
// simpler files with just two enums can crash.

// ================================================================
// BUG 17: [CRASH-CG] Array concatenation (+) crashes codegen
// ================================================================
// Using + to append to arrays crashes with LLVM assertion.
//
// REPRODUCTION:
//   var arr: [Int] = []
//   arr = arr + [10]
//
// Error: "Tried to create an integer operation on a non-integer type!"
// Note: Array literals and subscript access work. Only + crashes.
// Array elements after concat may also return garbage values.

// ================================================================
// BUG 18: [CRASH-CG] Nested struct type definitions crash
// ================================================================
// Struct type defined INSIDE another struct crashes.
//
// REPRODUCTION:
//   struct Outer {
//     struct Inner { var x: Int }
//     var inner: Inner
//   }
//
// WORKAROUND: Define structs separately, then compose:
//   struct Inner { var x: Int }
//   struct Outer { var inner: Inner }

// ================================================================
// BUG 19: [CRASH-CG] Computed properties crash codegen
// ================================================================
// Computed properties crash with LLVM debug info error.
//
// REPRODUCTION:
//   struct Temperature {
//     var celsius: Int
//     var fahrenheit: Int { return celsius * 9 / 5 + 32 }
//   }

// ================================================================
// BUG 20: [CRASH-CG] Generic functions crash codegen
// ================================================================
// Generic function instantiation crashes with LLVM assertion.
//
// REPRODUCTION:
//   func identity<T>(_ x: T) -> T { return x }
//   print(identity(42))
//
// Note: Generic structs (Box<T>) partially work but may lose
// field access (Box<Int>.value prints nothing).

// ================================================================
// BUG 21: [CRASH-CG] indirect enum crashes codegen
// ================================================================
// Recursive enum types marked with `indirect` crash.
//
// REPRODUCTION:
//   indirect enum List {
//     case cons(Int, List)
//     case nil_
//   }

// ================================================================
// BUG 22: [CRASH-CG] Closure expressions crash codegen
// ================================================================
// Closure literals crash the compiler.
//
// REPRODUCTION:
//   let add: (Int, Int) -> Int = { (a: Int, b: Int) -> Int in
//     return a + b
//   }
//
// Also crashes when passed as function argument.
// WORKAROUND: Use named functions instead.

// ================================================================
// BUG 23: [CRASH-CG] Function overloads with different types
// ================================================================
// Overloaded functions with different parameter types crash.
//
// REPRODUCTION:
//   func double(_ x: Int) -> Int { return x * 2 }
//   func double(_ x: String) -> String { return x }
//
// Overloads with same types but different parameter counts work.

// ================================================================
// BUG 24: [CRASH-CG] String comparison operators
// ================================================================
// Comparing strings with == or < crashes LLVM IR.
//
// REPRODUCTION:
//   let eq: Bool = "hello" == "hello"  // CRASH
//
// String switch works. Only direct comparison operators crash.

// ================================================================
// BUG 25: [CRASH-CG] Nested function capturing outer variable
// ================================================================
// Nested function accessing outer scope's parameter crashes.
//
// REPRODUCTION:
//   func makeAdder(_ base: Int) -> Int {
//     func add(_ x: Int) -> Int {
//       return base + x  // Captures `base`
//     }
//     return add(10)
//   }

// ================================================================
// BUG 26: [CRASH-SIL] if inside switch case arm
// ================================================================
// Using if/else inside a switch case body causes SIL error.
//
// REPRODUCTION:
//   switch state {
//   case .hasCoins(let amount):
//     if amount >= price {
//       return ...
//     }
//     return ...
//   }
//
// Error: "cond_br missing condition operand"
// WORKAROUND: Extract the if-logic into a separate function.

// ================================================================
// BUG 27: [CRASH-SIL] -> Void return type causes SIL errors
// ================================================================
// Explicit `-> Void` return type annotation causes SIL verification
// errors: "non-void function missing return on all paths".
//
// REPRODUCTION:
//   func doSomething() -> Void { print(42) }
//
// WORKAROUND: Omit the return type annotation entirely:
//   func doSomething() { print(42); return }

// ================================================================
// BUG 28: [SEMANTIC] guard let - bound variable undefined
// ================================================================
// Variables bound in guard-let are not accessible in the body.
//
// REPRODUCTION:
//   func test(_ x: Int?) -> Int {
//     guard let val = x else { return 0 }
//     return val  // ERROR: use of undefined name 'val'
//   }

// ================================================================
// BUG 29: [SEMANTIC] Generic enum associated value binding
// ================================================================
// Pattern matching on generic enum cases doesn't bind variables.
// Same issue affects Optional's .some(let x) pattern.
//
// REPRODUCTION:
//   enum Maybe<T> { case some(T); case none }
//   func unwrap(_ m: Maybe<Int>) -> Int {
//     switch m {
//     case .some(let v): return v  // ERROR: 'v' undefined
//     case .none: return 0
//     }
//   }
//
// Also: Int?.some(let x) doesn't bind x.
// WORKAROUND: Use force unwrap (!) or ?? instead.

// ================================================================
// BUG 30: [SEMANTIC] Generic struct field access fails
// ================================================================
// Accessing fields on monomorphized generic structs fails.
//
// REPRODUCTION:
//   struct Wrapper<T> { var item: T }
//   func getItem(_ w: Wrapper<Int>) -> Int {
//     return w.item  // ERROR: no member 'item'
//   }
//
// Note: Constructing and printing work. Only field access fails.

// ================================================================
// BUG 31: [SEMANTIC] Protocol method dispatch not supported
// ================================================================
// Calling methods through protocol-typed parameters fails.
//
// REPRODUCTION:
//   protocol Describable { func describe() -> String }
//   func show(_ d: Describable) {
//     print(d.describe())  // ERROR: no member 'describe'
//   }
//
// Protocol conformance on structs compiles but dispatch fails.

// ================================================================
// BUG 32: [SEMANTIC] Enum raw values not accessible
// ================================================================
// `.rawValue` property on raw-value enums is not recognized.
//
// REPRODUCTION:
//   enum Planet: Int { case earth = 3 }
//   print(Planet.earth.rawValue)  // ERROR: no member 'rawValue'

// ================================================================
// BUG 33: [SEMANTIC] Named argument labels with `from:to:` syntax
// ================================================================
// External parameter labels other than `_` may not be recognized.
//
// REPRODUCTION:
//   func range(from start: Int, to end: Int) -> Int {
//     return end - start
//   }
//   print(range(from: 3, to: 10))  // ERROR: incorrect argument label

// ================================================================
// BUG 34: [PARSE] Unary minus not supported
// ================================================================
// `-42` as a literal is not parsed. The parser expects an expression
// after `-`.
//
// REPRODUCTION:
//   let x: Int = -42  // ERROR: expected expression
//
// WORKAROUND: Use `0 - 42` instead.

// ================================================================
// BUG 35: [PARSE] ! prefix operator not supported
// ================================================================
// `!condition` is always tokenized as force unwrap (Exclaim), not
// as a prefix NOT operator.
//
// REPRODUCTION:
//   if !flag { ... }  // Parser crash
//
// WORKAROUND: Use `flag == false` instead.

// ================================================================
// BUG 36: [PARSE] Implicit member expressions not supported
// ================================================================
// `.memberName` shorthand for enum cases is not parsed as an
// expression (only works in switch case patterns).
//
// REPRODUCTION:
//   return .success(value)  // Parser crash
//
// WORKAROUND: Use full type name: `Result.success(value)`

// ================================================================
// BUG 37: [PARSE] Semicolons as statement separators in structs
// ================================================================
// Semicolons between declarations in struct bodies crash parser.
//
// REPRODUCTION:
//   struct Pair { var x: Int; var y: Int }
//
// WORKAROUND: Use newlines to separate declarations.

// ================================================================
// BUG 38: [MINOR] print(Bool) shows -1/0 instead of true/false
// ================================================================
// Boolean values print as -1 (true) and 0 (false).
//
// REPRODUCTION:
//   print(true)   // Prints -1
//   print(false)  // Prints 0

// ================================================================
// BUG 39: [MINOR] String interpolation with literals is empty
// ================================================================
// `\(42)` (literal) in string interpolation produces nothing,
// but `\(variable)` works.
//
// REPRODUCTION:
//   print("value: \(42)")  // Prints "value: " (missing 42)
//   let x: Int = 42
//   print("value: \(x)")   // Prints "value: 42" (works)

// ================================================================
// BUG 40: [MINOR] Multiline strings include delimiters/indentation
// ================================================================
// Triple-quoted strings don't strip leading indentation or delimiters.
//
// REPRODUCTION:
//   let s: String = """
//     hello
//     """
//   // Includes literal `"""` and leading spaces in output

// ================================================================
// BUG 41: [MINOR] Implicit return (single-expression body) fails
// ================================================================
// Functions with implicit return (no `return` keyword) produce wrong
// exit code.
//
// REPRODUCTION:
//   func square(_ x: Int) -> Int {
//     x * x  // No `return` keyword
//   }
//   // Compiles but exits with code 48 instead of correct result

// ================================================================
// BUG 42: [CRITICAL] Nested ternary operator evaluates wrong branch
// ================================================================
// When a ternary appears in the false branch of another ternary,
// the result is always the outer true-branch value.
// Even explicit parentheses don't fix it.
//
// REPRODUCTION:
//   let x: Int = 5
//   let r: Int = x > 10 ? 1 : x > 3 ? 2 : 3
//   // r = 1 (WRONG, should be 2)
//   let s: Int = (x > 10) ? 1 : ((x > 3) ? 2 : 3)
//   // s = 1 (WRONG, even with parens)
//
// Simple (non-nested) ternaries work correctly.
// WORKAROUND: Use sequential if-return statements.

// ================================================================
// BUG 43: [CRITICAL] String escape sequences not interpreted
// ================================================================
// Escape sequences \t, \n, \\, \" are printed as literal characters
// instead of being interpreted.
//
// REPRODUCTION:
//   print("a\tb")       // Prints literal "a\tb" not "a<TAB>b"
//   print("line1\nline2")  // Prints literal "line1\nline2"
//   print("a\\b")       // Prints literal "a\\b" not "a\b"
//   print("he said \"hi\"")  // Prints literal backslash-quote

// ================================================================
// BUG 44: [CRITICAL] String interpolation with expressions is empty
// ================================================================
// String interpolation with expressions (not just variables) produces
// empty string for the expression part.
//
// REPRODUCTION:
//   let x: Int = 5; let y: Int = 10
//   print("sum=\(x + y)")  // Prints "sum=" (missing 15)
//   print("\(x)")           // Works: prints variable value
//
// Related to Bug 39 (literal interpolation empty).
// WORKAROUND: Compute into a variable first, then interpolate.

// ================================================================
// BUG 45: [CRITICAL] No short-circuit evaluation for && and ||
// ================================================================
// Both operands are always evaluated regardless of left-hand value.
//
// REPRODUCTION:
//   func side() -> Bool { print("called"); return true }
//   let a: Bool = false && side()  // Prints "called" (should not)
//   let b: Bool = true || side()   // Prints "called" (should not)

// ================================================================
// BUG 46: [CRITICAL] switch where clause condition is ignored
// ================================================================
// `case let n where <condition>:` always matches regardless of condition.
//
// REPRODUCTION:
//   func classify(_ x: Int) -> Int {
//     switch x {
//     case let n where n > 100: return 3
//     case let n where n > 0: return 1
//     default: return 0
//     }
//   }
//   // classify(5) returns 3, expected 1
//   // classify(0) returns 3, expected 0

// ================================================================
// BUG 47: [CRASH-CG] Empty array literal crashes compiler
// ================================================================
// `let arr: [Int] = []` crashes with SemIR assertion error.
// Non-empty array literals work.
//
// REPRODUCTION:
//   let arr: [Int] = []
//
// Error: "CHECK failure: Casting inst {kind: ErrorInst} to wrong kind ClassType"

// ================================================================
// BUG 48: [SEMANTIC] for-in with inline array literal - var unbound
// ================================================================
// Loop variable is undefined when iterating over inline array literal.
// Named array variable works.
//
// REPRODUCTION:
//   for x in [1, 2, 3] { print(x) }  // ERROR: undefined 'x'
//   let arr = [1, 2, 3]; for x in arr { print(x) }  // Works

// ================================================================
// BUG 49: [SEMANTIC] Extension methods need explicit self. prefix
// ================================================================
// Extension methods can't access struct fields without `self.` prefix,
// but struct's own methods can.
//
// REPRODUCTION:
//   struct Box { var value: Int }
//   extension Box {
//     func doubled() -> Int { return value * 2 }  // ERROR
//   }
//
// WORKAROUND: Use self.value instead of value.

// ================================================================
// BUG 50: [PARSE] Semicolons fail in all code blocks (not just structs)
// ================================================================
// Extends Bug 37. Semicolons between statements crash parser in
// function bodies, if-bodies, while-bodies, and all other contexts.
//
// REPRODUCTION:
//   func main() -> Int {
//     let x: Int = 5; print(x)  // ERROR: expected expression
//     return 0
//   }

// ================================================================
// BUG 51: [CRASH-CG] Array.append() crashes codegen
// ================================================================
// .append() on var array crashes with LLVM function signature assertion.
//
// REPRODUCTION:
//   var arr: [Int] = [1, 2, 3]
//   arr.append(4)  // COMPILER CRASH

// ================================================================
// BUG 52: [LINK] Array.removeLast() generates undefined symbol
// ================================================================
// .removeLast() compiles to codegen but generates <unknown_callee>
// undefined reference at link time.
//
// REPRODUCTION:
//   var arr: [Int] = [1, 2, 3]
//   arr.removeLast()  // LINK ERROR

// ================================================================
// BUG 53: [SEMANTIC] for-in variable unbound with parameter array
// ================================================================
// for-in loop variable not bound when iterating a function parameter
// array. Local let arrays work fine.
//
// REPRODUCTION:
//   func sum(_ arr: [Int]) -> Int {
//     for x in arr { total += x }  // ERROR: undefined 'x'
//   }

// ================================================================
// BUG 54: [CRASH-CG] Returning array from function crashes
// ================================================================
// Functions that return [T] crash with SIL + LLVM assertions.
//
// REPRODUCTION:
//   func makeArr() -> [Int] { return [1, 2] }  // CRASH

// ================================================================
// BUG 55: [CRASH-CG] Nested array [[Int]] crashes codegen
// ================================================================
// Array of arrays crashes LLVM with pointer type assertion.
//
// REPRODUCTION:
//   let a: [[Int]] = [[1, 2], [3, 4]]  // CRASH

// ================================================================
// BUG 56: [CRITICAL] continue in for-in loop causes infinite loop
// ================================================================
// `continue` in for-in hangs forever. While-loop continue works.
//
// REPRODUCTION:
//   for i in 1...5 {
//     if i == 3 { continue }  // INFINITE LOOP
//     print(i)
//   }

// ================================================================
// BUG 57: [CRASH-CG] Mutating struct methods crash codegen
// ================================================================
// `mutating func` on structs crashes with SIL "return missing".
//
// REPRODUCTION:
//   struct Counter { var n: Int; mutating func inc() { n += 1 } }

// ================================================================
// BUG 58: [CRASH-CG] Static let properties crash codegen
// ================================================================
// `static let` crashes with debug info mismatch. Static methods work.
//
// REPRODUCTION:
//   struct Config { static let max: Int = 100 }  // CRASH

// ================================================================
// BUG 59: [CRASH-CG] Optional field in struct crashes codegen
// ================================================================
// Struct with `var field: T?` crashes LLVM ConstantAggregate assertion.
//
// REPRODUCTION:
//   struct W { var value: Int? }  // CRASH

// ================================================================
// BUG 60: [CRASH-CG] willSet/didSet property observers crash
// ================================================================
// Property observers crash with SIL + LLVM assertions.
//
// REPRODUCTION:
//   struct S { var x: Int { willSet { } } }  // CRASH

// ================================================================
// BUG 61: [CRASH-CG] Nested type definitions crash codegen
// ================================================================
// Struct defined inside another struct crashes. Separate composition works.
//
// REPRODUCTION:
//   struct Outer { struct Inner { var x: Int } }  // CRASH

// ================================================================
// BUG 62: [CRITICAL] `as` type cast silently produces no output
// ================================================================
// `x as Type` compiles but result prints nothing.
//
// REPRODUCTION:
//   let y: Int = 42 as Int; print(y)  // No output

// ================================================================
// BUG 63: [CRASH-RT] `is` type check crashes at runtime
// ================================================================
// `x is Type` compiles but segfaults when evaluated.
//
// REPRODUCTION:
//   if 42 is Int { print("yes") }  // SEGFAULT

// ================================================================
// BUG 64: [CRITICAL] Underscore in numeric literals → wrong value
// ================================================================
// `1_000_000` evaluates to 0 instead of 1000000.
//
// REPRODUCTION:
//   print(1_000_000)  // Prints 0

// ================================================================
// BUG 65: [CRASH-CG] Character type crashes codegen
// ================================================================
// `let c: Character = "A"` crashes with LLVM CastInst assertion.
//
// REPRODUCTION:
//   let c: Character = "A"  // CRASH

// ================================================================
// BUG 66: [CRASH-CG] Variadic parameters crash compiler
// ================================================================
// `func f(_ x: Int...)` crashes during codegen.
//
// REPRODUCTION:
//   func sum(_ nums: Int...) -> Int { ... }  // CRASH

// ================================================================
// BUG 67: [SEMANTIC+PARSE] Operator overloading not supported
// ================================================================
// Custom operator functions can't be declared (parse error for ==)
// and custom + for user types isn't resolved (type error).
//
// REPRODUCTION:
//   func +(l: V, r: V) -> V { ... }
//   let c = a + b  // ERROR: invalid operand types

// ================================================================
// BUG 68: [CRASH-CG] Subscript declarations crash codegen
// ================================================================
// `subscript(row: Int, col: Int) -> Int` crashes SIL + LLVM.
//
// REPRODUCTION:
//   struct G { subscript(i: Int) -> Int { return data[i] } }

// ================================================================
// BUG 69: [CRASH-CG] Enum named/multiple associated values crash
// ================================================================
// `case rect(width: Int, height: Int)` crashes codegen.
// Single unnamed associated values work.
//
// REPRODUCTION:
//   enum S { case rect(width: Int, height: Int) }  // CRASH

// ================================================================
// BUG 70: [LINK] Optional chaining ?.method() undefined reference
// ================================================================
// `value?.method()` generates <unknown_callee> at link time.
//
// REPRODUCTION:
//   let r = optionalBox?.doubled()  // LINK ERROR

// ================================================================
// BUG 71: [CRASH-CG] `if var` optional binding crashes codegen
// ================================================================
// `if var val = optional` crashes. `if let` works for simple cases.
//
// REPRODUCTION:
//   if var x = someOptional { x += 1 }  // CRASH

// ================================================================
// BUG 72: [CRASH-CG] Named tuple elements crash codegen
// ================================================================
// Tuples with named elements crash. Unnamed (.0, .1) partially work.
//
// REPRODUCTION:
//   func f() -> (sum: Int, diff: Int) { ... }  // CRASH

// ================================================================
// BUG 73: [CRASH-CG] lazy var properties crash compiler
// ================================================================
// `lazy var` crashes during codegen.
//
// REPRODUCTION:
//   struct S { lazy var x: Int = { 42 }() }  // CRASH

// ================================================================
// BUG 74: [SEMANTIC] String.uppercased()/lowercased() not callable
// ================================================================
// "cannot call non-function value" — exposed as properties not methods.
//
// REPRODUCTION:
//   "hello".uppercased()  // ERROR: cannot call non-function value

// ================================================================
// BUG 75: [CRASH-RT] String.contains/hasPrefix/hasSuffix segfault
// ================================================================
// String search methods compile but segfault at runtime.
//
// REPRODUCTION:
//   "hello".contains("ell")  // SEGFAULT

// ================================================================
// BUG 76: [CRASH-RT] String.isEmpty crashes at runtime
// ================================================================
// .isEmpty compiles but segfaults when accessed.
//
// REPRODUCTION:
//   if "".isEmpty { print("yes") }  // SEGFAULT

// ================================================================
// BUG 77: [CRASH-CG] Half-open range ..<  crashes codegen
// ================================================================
// `0..<5` crashes. Closed range `0...5` works.
//
// REPRODUCTION:
//   for i in 0..<5 { print(i) }  // CRASH

// ================================================================
// BUG 78: [CRASH-CG] Empty dictionary literal crashes
// ================================================================
// `[:]` crashes compiler. Non-empty String-keyed dicts work.
//
// REPRODUCTION:
//   let d: [String: Int] = [:]  // CRASH

// ================================================================
// BUG 79: [CRASH-CG] Dictionary with non-String key crashes
// ================================================================
// Int-keyed dicts crash. String keys work.
//
// REPRODUCTION:
//   let d: [Int: String] = [1: "one"]  // CRASH

// ================================================================
// BUG 80: [PARSE] Labeled loops not supported
// ================================================================
// `label: for` parsed as undefined name reference.
//
// REPRODUCTION:
//   outer: for i in 1...5 { break outer }  // ERROR

// ================================================================
// BUG 81: [CRASH-CG] inout with array element crashes
// ================================================================
// `&arr[i]` crashes. Plain `&variable` works.
//
// REPRODUCTION:
//   increment(&arr[1])  // CRASH

// ================================================================
// BUG 82: [CRASH-CG] Array of optionals crashes codegen
// ================================================================
// `[Int?]` containing nil crashes LLVM type assertion.
//
// REPRODUCTION:
//   let arr: [Int?] = [1, nil, 3]  // CRASH

// ================================================================
// BUG 83: [CRITICAL] Multiple guard statements fail
// ================================================================
// Sequential guards don't check conditions correctly.
// Second+ guards always fall through.
//
// REPRODUCTION:
//   guard x > 0 else { return "bad" }
//   guard y > 0 else { return "bad" }  // Never triggers

// ================================================================
// BUG 84: [CRASH-CG] Switch on tuple patterns crashes
// ================================================================
// `switch (x, y) { case (0, 0): ... }` crashes LLVM.
//
// REPRODUCTION:
//   switch (x, y) { case (0, 0): ... }  // CRASH

// ================================================================
// BUG 85: [CRITICAL] defer in loop body doesn't execute per-iteration
// ================================================================
// defer inside while/for doesn't run at end of each iteration.
//
// REPRODUCTION:
//   while i < 3 { defer { print("d") }; i += 1 }
//   // "d" never prints

// ================================================================
// BUG 86: [CRITICAL] print() multi-args/separator/terminator broken
// ================================================================
// print(1,2,3) only prints first arg. separator/terminator ignored.
//
// REPRODUCTION:
//   print(1, 2, 3)  // Prints "1" only
//   print("hi", terminator: "")  // Still adds newline

// ================================================================
// BUG 87: [CRASH-CG] String indexing crashes codegen
// ================================================================
// startIndex, subscript, index(offsetBy:) all crash.
//
// REPRODUCTION:
//   let i = s.startIndex; print(s[i])  // CRASH

// ================================================================
// BUG 88: [CRASH-CG] Ternary in string interpolation crashes
// ================================================================
// `\(cond ? "a" : "b")` crashes LLVM StringRef assertion.
//
// REPRODUCTION:
//   print("\(x > 0 ? "pos" : "neg")")  // CRASH

// ================================================================
// BUG 89: [CRASH-CG] Tuple swap assignment crashes
// ================================================================
// `(a, b) = (b, a)` crashes. Destructuring `let (a,b) = (1,2)` works.
//
// REPRODUCTION:
//   (a, b) = (b, a)  // CRASH

// ================================================================
// BUG 90: [SEMANTIC] if-let with multiple bindings doesn't bind
// ================================================================
// `if let a = x, let b = y { ... }` — variables not accessible.
//
// REPRODUCTION:
//   if let a = x, let b = y { print(a) }  // ERROR: undefined 'a'

// ================================================================
// BUG 91: [CRASH-CG] Enum equality (==, !=) crashes codegen
// ================================================================
// Comparing enum values crashes LLVM ICmpInst type assertion.
//
// REPRODUCTION:
//   if Color.red == Color.blue { ... }  // CRASH

// ================================================================
// BUG 92: [CRITICAL] nil comparison (== nil, != nil) fails silently
// ================================================================
// `x == nil` compiles but produces no output, exits with code 48.
//
// REPRODUCTION:
//   let x: Int? = nil; if x == nil { print("nil") }  // No output

// ================================================================
// BUG 93: [CRITICAL] for-in variable leaks into outer scope
// ================================================================
// Loop variable shadows outer name but leaks after loop ends.
//
// REPRODUCTION:
//   let i = 99; for i in 1...3 { }; print(i)  // Prints 3, not 99

// ================================================================
// BUG 94: [CRASH-RT] String equality (==, !=) segfaults at runtime
// ================================================================
// String comparison compiles but crashes.
//
// REPRODUCTION:
//   if "hello" == "hello" { print("same") }  // SEGFAULT

// ================================================================
// BUG 95: [CRASH-CG] Double/Float types crash codegen
// ================================================================
// Any use of Double or Float crashes with LLVM cast assertion.
//
// REPRODUCTION:
//   let x: Double = 3.14  // CRASH
//   let y: Float = 2.5    // CRASH
