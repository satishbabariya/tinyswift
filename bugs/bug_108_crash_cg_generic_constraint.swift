// BUG 108: [CRASH-CG] Generic functions with where/protocol constraints crash
//
// Generic functions using protocol constraints (e.g., `<T: Equatable>`)
// crash the compiler with a debug info attachment error.
//
// Related to Bug 20 (generic functions crash) but specifically about
// constrained generics, which produce a different error.
//
// EXPECTED: compiles and prints "equal"
// ACTUAL: compiler crash (debug info assertion)
//
// Build: tinyswift compile --no-prelude-import bugs/bug_108_crash_cg_generic_constraint.swift

func isEqual<T: Equatable>(_ a: T, _ b: T) -> Bool {
  return a == b                    // COMPILER CRASH
}

func main() -> Int {
  if isEqual(5, 5) { print("equal") }
  return 0
}
