// BUG 29: [SEMANTIC] Generic enum associated value binding
//
// Pattern matching on generic enum cases doesn't bind variables.
// Same issue affects Optional's .some(let x) pattern.
//
// EXPECTED: compiles and prints 42
// ACTUAL: ERROR: 'v' undefined
//
// Also: Int?.some(let x) doesn't bind x.
// WORKAROUND: Use force unwrap (!) or ?? instead.
//
// Build: tinyswift compile --no-prelude-import bugs/bug_29_semantic_generic_enum_binding.swift

enum Maybe<T> {
  case some(T)
  case none
}

func unwrap(_ m: Maybe<Int>) -> Int {
  switch m {
  case .some(let v): return v  // ERROR: 'v' undefined
  case .none: return 0
  }
}

func main() -> Int {
  let m: Maybe<Int> = Maybe<Int>.some(42)
  print(unwrap(m))
  return 0
}
