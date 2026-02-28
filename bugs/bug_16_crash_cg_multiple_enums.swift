// BUG 16: [CRASH-CG] Multiple enum types in one file
//
// Having two or more enum type definitions in the same file crashes.
//
// Note: This crash is intermittent and may depend on file complexity.
// The compiler_test_suite.swift has multiple enums and works, but
// simpler files with just two enums can crash.
//
// Build: tinyswift compile --no-prelude-import bugs/bug_16_crash_cg_multiple_enums.swift

enum A {
  case x
  case y
}

enum B {
  case m
  case n
}

func main() -> Int {
  print(0)
  return 0
}
