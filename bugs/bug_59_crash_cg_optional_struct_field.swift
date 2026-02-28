// BUG 59: [CRASH-CG] Struct with optional field crashes codegen
//
// Declaring a struct that has an optional-typed field (e.g., `var value: Int?`)
// crashes the compiler with an LLVM ConstantAggregate assertion about
// struct element type mismatch.
//
// EXPECTED: compiles and prints "10\n0"
// ACTUAL: compiler crash (LLVM type assertion)
//
// Build: tinyswift compile --no-prelude-import bugs/bug_59_crash_cg_optional_struct_field.swift

struct Wrapper {
  var value: Int?     // COMPILER CRASH
}

func main() -> Int {
  let a: Wrapper = Wrapper(value: 10)
  let b: Wrapper = Wrapper(value: nil)
  print(a.value ?? 0)
  print(b.value ?? 0)
  return 0
}
