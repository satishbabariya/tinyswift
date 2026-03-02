// BUG 73: [CRASH-CG] lazy var properties crash compiler
//
// Declaring a `lazy var` property crashes the compiler during codegen.
//
// EXPECTED: compiles and prints 42
// ACTUAL: compiler crash
//
// Build: tinyswift compile --no-prelude-import bugs/bug_73_crash_cg_lazy_var.swift

struct Heavy {
  lazy var computed: Int = {    // COMPILER CRASH
    return 42
  }()
}

func main() -> Int {
  var h: Heavy = Heavy()
  print(h.computed)
  return 0
}
