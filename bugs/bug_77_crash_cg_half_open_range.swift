// BUG 77: [CRASH-CG] Half-open range operator (..<) crashes codegen
//
// Using the half-open range operator `..<` crashes the compiler during
// codegen. The closed range operator `...` works correctly.
//
// EXPECTED: compiles and prints 10
// ACTUAL: compiler crash
//
// WORKAROUND: Use closed range `0...4` instead of `0..<5`.
//
// Build: tinyswift compile --no-prelude-import bugs/bug_77_crash_cg_half_open_range.swift

func main() -> Int {
  var sum: Int = 0
  for i in 0..<5 {         // COMPILER CRASH
    sum = sum + i
  }
  print(sum)
  return 0
}

// Workaround using closed range:
// for i in 0...4 { sum = sum + i }  // Works
