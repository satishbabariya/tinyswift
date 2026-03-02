// BUG 125: [CRASH] Generic functions and types crash codegen
//
// Generic functions crash with LLVM CallInst assertion or !dbg errors.
// Generic structs fail to resolve member init fields.
//
// EXPECTED: prints 42
// ACTUAL: Assertion "Calling a function with a bad signature!" / !dbg error
//
// WORKAROUND: Write type-specific overloads instead of generics.

func identity<T>(_ x: T) -> T {   // CRASH
  return x
}

func main() -> Int {
  print(identity(42))
  return 0
}
