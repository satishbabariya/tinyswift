// BUG 149: [CRASH] Mutating method with array subscript on self crashes
//
// A mutating method that writes to self's array field via subscript
// crashes with SIL verification error ("return missing operand") and
// StoreInst assertion.
//
// EXPECTED: prints 3 then 30
// ACTUAL: SIL verification error + assertion crash
//
// WORKAROUND: Avoid array subscript writes inside mutating methods.
// Use free functions with inout parameters or restructure data.

struct Stack {
  var items: [Int]
  var size: Int
  mutating func push(_ val: Int) {
    self.items[self.size] = val   // CRASH
    self.size = self.size + 1
  }
}

func main() -> Int {
  var s: Stack = Stack(items: [0, 0, 0, 0, 0], size: 0)
  s.push(10)
  s.push(20)
  s.push(30)
  print(s.size)
  return 0
}
