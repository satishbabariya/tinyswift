// BUG 112: [SEMANTIC] .reversed() on ranges not supported
//
// Calling .reversed() on a range expression like `(1...5).reversed()`
// fails. The for-in loop variable is unbound ("use of undefined name").
//
// EXPECTED: prints 5 4 3 2 1 (each on new line)
// ACTUAL: error: use of undefined name 'i'
//
// WORKAROUND: Use a while loop with a decrementing counter:
//   var i: Int = 5
//   while i >= 1 { print(i); i = i - 1 }
//
// Build: tinyswift compile --no-prelude-import --output=OUT bugs/bug_112_semantic_reversed_range.swift

func main() -> Int {
  for i in (1...5).reversed() {   // ERROR: undefined 'i'
    print(i)
  }
  return 0
}
