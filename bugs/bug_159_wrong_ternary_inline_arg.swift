// BUG 159: [WRONG] Ternary expression as function argument produces garbage
//
// Passing a ternary expression (with String values) directly as a
// function argument produces a garbage pointer value instead of the
// correct string. Storing the ternary result in a variable first works.
//
// EXPECTED: prints "big"
// ACTUAL: prints garbage number like 93898082119680 (pointer value)
//
// WORKAROUND: Store ternary result in a variable first:
//   let s: String = x > 3 ? "big" : "small"
//   print(s)

func main() -> Int {
  let x: Int = 5
  print(x > 3 ? "big" : "small")   // Garbage pointer output
  return 0
}
