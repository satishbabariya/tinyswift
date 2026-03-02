// BUG 89: [CRASH-CG] Tuple swap assignment crashes codegen
//
// The tuple swap pattern `(a, b) = (b, a)` crashes the compiler.
// Plain tuple destructuring `let (a, b) = (1, 2)` works.
//
// EXPECTED: prints "2\n1"
// ACTUAL: compiler crash (LLVM StoreInst pointer type assertion)
//
// Build: tinyswift compile --no-prelude-import bugs/bug_89_crash_cg_tuple_swap.swift

func main() -> Int {
  var a: Int = 1
  var b: Int = 2
  (a, b) = (b, a)      // COMPILER CRASH
  print(a)
  print(b)
  return 0
}
