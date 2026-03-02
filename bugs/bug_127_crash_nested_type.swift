// BUG 127: [CRASH] Nested types (struct in struct) crash codegen
//
// Defining a struct inside another struct compiles, but using the nested
// type causes a ConstantAggregate assertion failure.
//
// EXPECTED: prints 42
// ACTUAL: Assertion "Initializer for struct element doesn't match!"
//
// WORKAROUND: Define all types at file scope (no nesting).

struct Outer {
  struct Inner {
    var value: Int
  }
  var inner: Inner
}

func main() -> Int {
  let o: Outer = Outer(inner: Outer.Inner(value: 42))
  print(o.inner.value)
  return 0
}
