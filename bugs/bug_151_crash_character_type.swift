// BUG 151: [CRASH] Character type crashes codegen
//
// Declaring a variable of type Character crashes with a CastInst
// assertion during codegen.
//
// EXPECTED: prints "A"
// ACTUAL: Assertion "Invalid cast!"
//
// WORKAROUND: Use String type instead of Character.

func main() -> Int {
  let c: Character = "A"   // CRASH
  print(c)
  return 0
}
