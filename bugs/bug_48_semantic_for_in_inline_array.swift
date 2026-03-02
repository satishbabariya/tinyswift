// BUG 48: [SEMANTIC] for-in with inline array literal doesn't bind loop variable
//
// When using an inline array literal directly in a for-in loop,
// the loop variable is not bound and produces "undefined name" error.
// Using a named array variable works correctly.
//
// EXPECTED: prints 10, 20, 30
// ACTUAL: error: use of undefined name 'x'
//
// WORKAROUND: Assign the array to a named variable first.
//
// Build: tinyswift compile --no-prelude-import bugs/bug_48_semantic_for_in_inline_array.swift

func main() -> Int {
  // This fails:
  for x in [10, 20, 30] {
    print(x)  // ERROR: use of undefined name 'x'
  }

  // Workaround - named array works:
  // let arr: [Int] = [10, 20, 30]
  // for x in arr { print(x) }  // Works

  return 0
}
