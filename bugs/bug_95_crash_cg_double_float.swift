// BUG 95: [CRASH-CG] Double and Float types crash codegen
//
// Declaring or using Double or Float values crashes the compiler
// with LLVM CastInst "Invalid cast!" assertion. Same underlying
// error as Character type (Bug 65).
//
// EXPECTED: compiles and prints 3.14
// ACTUAL: compiler crash (LLVM cast assertion)
//
// Build: tinyswift compile --no-prelude-import bugs/bug_95_crash_cg_double_float.swift

func main() -> Int {
  let x: Double = 3.14        // COMPILER CRASH
  print(x)
  return 0
}

// Also crashes:
// let y: Float = 2.5
// let z: Double = 1.5 + 2.5
