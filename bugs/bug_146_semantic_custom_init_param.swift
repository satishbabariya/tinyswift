// BUG 146: [SEMANTIC] Custom init with _ parameter name not bound
//
// In `init(_ x: Int)`, the internal parameter name `x` is not available
// in the init body. The parser doesn't correctly handle `_` as an
// external parameter label in init declarations.
//
// EXPECTED: prints 5 then 10
// ACTUAL: error: use of undefined name 'x'
//
// WORKAROUND: Use labeled parameter: `init(x: Int)` then `Pair(x: 5)`.

struct Pair {
  var a: Int
  var b: Int
  init(_ x: Int) {      // Parser doesn't bind 'x'
    self.a = x           // ERROR: use of undefined name 'x'
    self.b = x * 2
  }
}

func main() -> Int {
  let p: Pair = Pair(5)
  print(p.a)
  print(p.b)
  return 0
}
