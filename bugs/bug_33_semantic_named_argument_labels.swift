// BUG 33: [SEMANTIC] Named argument labels with `from:to:` syntax
//
// External parameter labels other than `_` may not be recognized
// at the call site.
//
// EXPECTED: prints 7
// ACTUAL: ERROR: incorrect argument label
//
// Build: tinyswift compile --no-prelude-import bugs/bug_33_semantic_named_argument_labels.swift

func range(from start: Int, to end: Int) -> Int {
  return end - start
}

func main() -> Int {
  print(range(from: 3, to: 10))  // ERROR: incorrect argument label
  return 0
}
