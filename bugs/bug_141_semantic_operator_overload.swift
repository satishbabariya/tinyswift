// BUG 141: [SEMANTIC] Custom operator overloading not recognized
//
// Defining a free function `+` for custom types compiles, but using
// the operator in expressions fails with "invalid operand types."
// The compiler doesn't look up custom operator functions.
//
// EXPECTED: prints 4, 6
// ACTUAL: error: invalid operand types for '+': 'Vec' and 'Vec'
//
// WORKAROUND: Use a named function (e.g., func add(_ a: Vec, _ b: Vec)).

struct Vec {
  var x: Int
  var y: Int
}

func +(lhs: Vec, rhs: Vec) -> Vec {
  return Vec(x: lhs.x + rhs.x, y: lhs.y + rhs.y)
}

func main() -> Int {
  let a: Vec = Vec(x: 1, y: 2)
  let b: Vec = Vec(x: 3, y: 4)
  let c: Vec = a + b           // ERROR
  print(c.x)
  print(c.y)
  return 0
}
