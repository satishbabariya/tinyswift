// BUG 86: [CRITICAL] print() with multiple arguments only prints first
//
// Three related issues:
// 1. print(1, 2, 3) only prints "1" — subsequent arguments ignored
// 2. print(1, 2, 3, separator: " ") — separator parameter ignored
// 3. print("hi", terminator: "") — terminator parameter ignored,
//    always appends newline
//
// EXPECTED: "1 2 3"
// ACTUAL: "1"
//
// Build: tinyswift compile --no-prelude-import bugs/bug_86_critical_print_multi_args.swift
// Run:   ./bug_86_critical_print_multi_args

func main() -> Int {
  // Only prints first argument:
  print(1, 2, 3)              // Prints "1" instead of "1 2 3"

  // Separator is ignored:
  print(1, 2, 3, separator: " ")  // Still prints "1"

  // Terminator is ignored:
  print("hello", terminator: "")  // Prints "hello\n" (newline always added)
  print(" world")

  return 0
}
