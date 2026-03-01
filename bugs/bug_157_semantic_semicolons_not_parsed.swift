// BUG 157: [SEMANTIC] Semicolons as statement separators not recognized
//
// Multiple statements on one line separated by semicolons cause
// "expected expression" errors. The parser does not recognize `;`
// as a statement terminator/separator.
//
// EXPECTED: prints 3
// ACTUAL: error: expected expression (at semicolon)
//
// WORKAROUND: Put each statement on its own line.

func main() -> Int {
  let a: Int = 1; let b: Int = 2    // ERROR at semicolon
  print(a + b)
  return 0
}
