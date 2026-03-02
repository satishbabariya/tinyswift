// BUG 134: [CRASH] Class types crash at runtime (segfault)
//
// Class instances compile but segfault when accessed at runtime.
// The init and field access cause a runtime crash (signal 139).
//
// EXPECTED: prints 42
// ACTUAL: segfault (rc=139)
//
// WORKAROUND: Use struct types instead of classes.

class Obj {
  var x: Int
  init(x: Int) { self.x = x }
}

func main() -> Int {
  let o: Obj = Obj(x: 42)
  print(o.x)   // SEGFAULT
  return 0
}
