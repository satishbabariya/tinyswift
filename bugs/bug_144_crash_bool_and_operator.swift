// BUG 144: [CRASH] Boolean && operator crashes codegen
//
// Using the && (logical AND) operator in conditions causes a stack dump
// during compilation. Individual comparisons work fine.
//
// EXPECTED: prints "first only"
// ACTUAL: compiler crash (stack dump)
//
// WORKAROUND: Use nested if-statements instead of &&.
//   if a { if b { ... } }

func main() -> Int {
  let a: Bool = true
  let b: Bool = false
  if a && b { print("both") }       // CRASH
  if a && !b { print("first only") }
  return 0
}
