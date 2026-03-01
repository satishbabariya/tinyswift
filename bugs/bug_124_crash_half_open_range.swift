// BUG 124: [CRASH] Half-open range operator (..<) crashes codegen
//
// The half-open range operator `..<` causes a stack dump during compilation.
// The closed range operator `...` works correctly.
//
// EXPECTED: prints 0, 1, 2
// ACTUAL: compiler crash (stack dump)
//
// WORKAROUND: Use closed range `...` with adjusted upper bound:
//   for i in 0...(n-1) { ... }

func main() -> Int {
  for i in 0..<3 {   // CRASH
    print(i)
  }
  return 0
}
