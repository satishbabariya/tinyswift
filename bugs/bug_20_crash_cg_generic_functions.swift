// BUG 20: [CRASH-CG] Generic functions crash codegen
//
// Generic function instantiation crashes with LLVM assertion.
//
// EXPECTED: prints 42
// ACTUAL: CRASH during codegen
//
// Note: Generic structs (Box<T>) partially work but may lose
// field access (Box<Int>.value prints nothing).
//
// Build: tinyswift compile --no-prelude-import bugs/bug_20_crash_cg_generic_functions.swift

func identity<T>(_ x: T) -> T {
  return x
}

func main() -> Int {
  print(identity(42))
  return 0
}
