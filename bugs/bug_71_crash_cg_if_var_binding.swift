// BUG 71: [CRASH-CG] `if var` optional binding crashes codegen
//
// Using `if var` (instead of `if let`) to create a mutable binding
// from an optional crashes the compiler with LLVM StoreInst assertion.
//
// `if let` works for simple cases but `if var` always crashes.
//
// EXPECTED: compiles and prints 15
// ACTUAL: compiler crash (LLVM pointer type assertion)
//
// Build: tinyswift compile --no-prelude-import bugs/bug_71_crash_cg_if_var_binding.swift

func main() -> Int {
  var x: Int? = 5
  if var val = x {        // COMPILER CRASH
    val = val + 10
    print(val)
  }
  return 0
}
