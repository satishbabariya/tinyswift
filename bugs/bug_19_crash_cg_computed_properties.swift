// BUG 19: [CRASH-CG] Computed properties crash codegen
//
// Computed properties crash with LLVM debug info error.
//
// EXPECTED: prints 212 (100 * 9 / 5 + 32)
// ACTUAL: CRASH during codegen
//
// Build: tinyswift compile --no-prelude-import bugs/bug_19_crash_cg_computed_properties.swift

struct Temperature {
  var celsius: Int
  var fahrenheit: Int { return celsius * 9 / 5 + 32 }
}

func main() -> Int {
  let t: Temperature = Temperature(celsius: 100)
  print(t.fahrenheit)
  return 0
}
