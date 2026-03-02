// BUG 74: [SEMANTIC] String.uppercased()/lowercased() not callable
//
// The String type has uppercased and lowercased members but they cannot
// be called as functions. The compiler reports "cannot call non-function
// value", suggesting they are exposed as properties rather than methods.
//
// EXPECTED: prints "HELLO"
// ACTUAL: error: cannot call non-function value
//
// Build: tinyswift compile --no-prelude-import bugs/bug_74_semantic_string_case_methods.swift

func main() -> Int {
  let s: String = "hello"
  print(s.uppercased())    // ERROR: cannot call non-function value
  return 0
}

// Also affects:
// s.lowercased()  → same error
