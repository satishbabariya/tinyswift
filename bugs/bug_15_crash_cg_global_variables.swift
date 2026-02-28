// BUG 15: [CRASH-CG] Global variables crash codegen
//
// Top-level var declarations crash with LLVM debug info error.
//
// EXPECTED: prints 1
// ACTUAL: CRASH: "!dbg attachment points at wrong subprogram for function"
//
// Build: tinyswift compile --no-prelude-import bugs/bug_15_crash_cg_global_variables.swift

var counter: Int = 0

func main() -> Int {
  counter = counter + 1
  print(counter)
  return 0
}
