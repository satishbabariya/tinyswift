// BUG 18: [CRASH-CG] Nested struct type definitions crash
//
// Struct type defined INSIDE another struct crashes codegen.
//
// EXPECTED: compiles and runs
// ACTUAL: CRASH during codegen
//
// WORKAROUND: Define structs separately, then compose:
//   struct Inner { var x: Int }
//   struct Outer { var inner: Inner }
//
// Build: tinyswift compile --no-prelude-import bugs/bug_18_crash_cg_nested_struct.swift

struct Outer {
  struct Inner {
    var x: Int
  }
  var inner: Inner
}

func main() -> Int {
  let o: Outer = Outer(inner: Outer.Inner(x: 42))
  print(o.inner.x)
  return 0
}
