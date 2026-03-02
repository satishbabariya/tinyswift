// BUG 87: [CRASH-CG] String indexing crashes codegen
//
// Accessing string characters via index operations (startIndex,
// subscript, index(offsetBy:)) crashes the compiler.
//
// EXPECTED: compiles and prints "h"
// ACTUAL: compiler crash
//
// Build: tinyswift compile --no-prelude-import bugs/bug_87_crash_cg_string_indexing.swift

func main() -> Int {
  let s: String = "hello"
  let i = s.startIndex           // COMPILER CRASH
  print(s[i])
  return 0
}
