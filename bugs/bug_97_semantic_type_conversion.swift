// BUG 97: [SEMANTIC] Type conversion initializers not callable
//
// Type conversion initializers like String(intValue) and Int(stringValue)
// are not supported. String() reports "cannot call non-function value"
// and Int() reports "use of undefined name 'Int'".
//
// EXPECTED: prints "42"
// ACTUAL: error: cannot call non-function value
//
// Build: tinyswift compile --no-prelude-import bugs/bug_97_semantic_type_conversion.swift

func main() -> Int {
  let x: Int = 42
  let s: String = String(x)     // ERROR: cannot call non-function value
  print(s)

  // Also broken:
  // let n: Int? = Int("42")    // ERROR: use of undefined name 'Int'

  return 0
}
