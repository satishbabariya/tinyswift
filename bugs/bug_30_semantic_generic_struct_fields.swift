// BUG 30: [SEMANTIC] Generic struct field access fails
//
// Accessing fields on monomorphized generic structs fails.
//
// EXPECTED: compiles and prints 42
// ACTUAL: ERROR: no member 'item'
//
// Note: Constructing and printing generic structs work.
// Only field access fails.
//
// Build: tinyswift compile --no-prelude-import bugs/bug_30_semantic_generic_struct_fields.swift

struct Wrapper<T> {
  var item: T
}

func getItem(_ w: Wrapper<Int>) -> Int {
  return w.item  // ERROR: no member 'item'
}

func main() -> Int {
  let w: Wrapper<Int> = Wrapper<Int>(item: 42)
  print(getItem(w))
  return 0
}
