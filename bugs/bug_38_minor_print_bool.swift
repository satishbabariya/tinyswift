// BUG 38: [MINOR] print(Bool) shows -1/0 instead of true/false
//
// Boolean values print as -1 (true) and 0 (false) instead of
// the expected "true" and "false" strings.
//
// EXPECTED: prints "true" then "false"
// ACTUAL: prints "-1" then "0"
//
// Build: tinyswift compile --no-prelude-import bugs/bug_38_minor_print_bool.swift
// Run:   ./bug_38_minor_print_bool

func main() -> Int {
  print(true)   // Prints -1 instead of true
  print(false)  // Prints 0 instead of false
  return 0
}
