// BUG 40: [MINOR] Multiline strings include delimiters/indentation
//
// Triple-quoted strings don't strip leading indentation or delimiters
// as expected by Swift specification.
//
// EXPECTED: prints "hello" (stripped)
// ACTUAL: includes literal `"""` and leading spaces in output
//
// Build: tinyswift compile --no-prelude-import bugs/bug_40_minor_multiline_strings.swift
// Run:   ./bug_40_minor_multiline_strings

func main() -> Int {
  let s: String = """
    hello
    """
  print(s)  // Includes literal """ and leading spaces
  return 0
}
