// BUG 67: [SEMANTIC+PARSE] Operator overloading not supported
//
// Two related issues:
// 1. Custom operator functions using symbols (func ==, func +) cannot be
//    parsed - the parser rejects operator symbols as function names.
// 2. Even if a free function with matching types exists, the compiler
//    doesn't resolve custom operators for user-defined types.
//
// Issue 1 - Parse error:
//   func ==(lhs: Point, rhs: Point) -> Bool { ... }
//   ERROR: expected identifier in declaration
//
// Issue 2 - Type error:
//   func +(lhs: Vec2, rhs: Vec2) -> Vec2 { ... }
//   let c = a + b  // ERROR: invalid operand types for '+': 'Vec2' and 'Vec2'
//
// Build: tinyswift compile --no-prelude-import bugs/bug_67_semantic_operator_overload.swift

struct Vec2 {
  var x: Int
  var y: Int
}

func +(lhs: Vec2, rhs: Vec2) -> Vec2 {
  return Vec2(x: lhs.x + rhs.x, y: lhs.y + rhs.y)
}

func main() -> Int {
  let a: Vec2 = Vec2(x: 1, y: 2)
  let b: Vec2 = Vec2(x: 3, y: 4)
  let c: Vec2 = a + b    // ERROR: invalid operand types
  print(c.x)
  print(c.y)
  return 0
}
