// BUG 66: [CRASH-CG] Variadic parameters crash compiler
//
// Declaring a function with variadic parameters (Type...) crashes
// the compiler during codegen.
//
// EXPECTED: compiles and prints 15
// ACTUAL: compiler crash
//
// Build: tinyswift compile --no-prelude-import bugs/bug_66_crash_cg_variadic_params.swift

func sum(_ nums: Int...) -> Int {   // COMPILER CRASH
  var total: Int = 0
  for n in nums {
    total = total + n
  }
  return total
}

func main() -> Int {
  print(sum(1, 2, 3, 4, 5))
  return 0
}
