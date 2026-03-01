// BUG 132: [WRONG] Switch fallthrough doesn't fall through
//
// The `fallthrough` keyword in switch cases is parsed but has no effect.
// Control does NOT transfer to the next case; instead execution continues
// after the switch as if fallthrough was not present.
//
// EXPECTED: prints "A" then "B" (fallthrough to case 2)
// ACTUAL: prints "A" only
//
// WORKAROUND: Duplicate the code from subsequent cases manually.

func main() -> Int {
  let x: Int = 1
  var result: String = ""
  switch x {
  case 1:
    result = result + "A"
    fallthrough            // Does NOT fall through
  case 2:
    result = result + "B"
  default:
    result = result + "C"
  }
  print(result)
  return 0
}
