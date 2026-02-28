// BUG 61: [CRASH-CG] Nested type definitions crash codegen
//
// Defining a struct (or enum) inside another struct crashes the compiler
// with an LLVM ConstantAggregate assertion about struct element type mismatch.
//
// Composing separately-defined structs (struct A uses struct B as field)
// works fine. Only nested type definitions (struct defined inside struct)
// crash.
//
// EXPECTED: compiles and prints 42
// ACTUAL: compiler crash (LLVM type assertion)
//
// Build: tinyswift compile --no-prelude-import bugs/bug_61_crash_cg_nested_type_def.swift

struct Outer {
  struct Inner {        // Nested type definition
    var x: Int
  }
  var inner: Inner
}

func main() -> Int {
  let o: Outer = Outer(inner: Outer.Inner(x: 42))
  print(o.inner.x)
  return 0
}
