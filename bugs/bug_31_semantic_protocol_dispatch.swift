// BUG 31: [SEMANTIC] Protocol method dispatch not supported
//
// Calling methods through protocol-typed parameters fails.
//
// EXPECTED: compiles and prints description
// ACTUAL: ERROR: no member 'describe'
//
// Protocol conformance on structs compiles but dispatch fails.
//
// Build: tinyswift compile --no-prelude-import bugs/bug_31_semantic_protocol_dispatch.swift

protocol Describable {
  func describe() -> String
}

struct Dog: Describable {
  func describe() -> String {
    return "Dog"
  }
}

func show(_ d: Describable) {
  print(d.describe())  // ERROR: no member 'describe'
  return
}

func main() -> Int {
  let dog: Dog = Dog()
  show(dog)
  return 0
}
