// BUG 126: [CRASH] Type alias for tuple type crashes codegen
//
// Defining a typealias for a tuple type compiles, but using it as a
// function parameter type causes SIL verification error and CallInst crash.
//
// EXPECTED: prints 30
// ACTUAL: SIL verification error: return missing operand / CallInst assertion
//
// WORKAROUND: Use the raw tuple type directly without typealias.

typealias IntPair = (Int, Int)

func sum(_ p: IntPair) -> Int {
  return p.0 + p.1
}

func main() -> Int {
  let p: IntPair = (10, 20)
  print(sum(p))
  return 0
}
