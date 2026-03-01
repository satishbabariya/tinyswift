// BUG 122: [CRASH] Computed properties crash codegen
//
// Read-only computed properties (with implicit or explicit get) cause
// debug info metadata errors or SIL verification failures during codegen.
//
// EXPECTED: compiles and prints 75
// ACTUAL: !dbg attachment points at wrong subprogram for function
//
// WORKAROUND: Use methods instead of computed properties.

struct Circle {
  var radius: Int
  var area: Int {           // CRASH: codegen metadata error
    return radius * radius * 3
  }
}

func main() -> Int {
  let c: Circle = Circle(radius: 5)
  print(c.area)
  return 0
}
