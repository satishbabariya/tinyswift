// BUG 58: [CRASH-CG] Static let properties crash codegen
//
// Declaring `static let` on a struct crashes the compiler with a debug
// info mismatch error ("!dbg attachment points at wrong subprogram").
// Static methods (static func) work correctly.
//
// EXPECTED: compiles and prints 100
// ACTUAL: compiler crash (LLVM debug info assertion)
//
// Build: tinyswift compile --no-prelude-import bugs/bug_58_crash_cg_static_property.swift

struct Config {
  static let maxSize: Int = 100   // COMPILER CRASH
}

func main() -> Int {
  print(Config.maxSize)
  return 0
}
