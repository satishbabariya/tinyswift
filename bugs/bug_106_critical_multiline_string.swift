// BUG 106: [CRITICAL] Multiline strings """ parsed incorrectly
//
// Triple-quoted multiline strings are not parsed correctly. The
// content includes the opening quotes and indentation instead of
// the actual string content.
//
// EXPECTED: prints "hello\nworld"
// ACTUAL: prints '""' and '  hello' (quotes and indent included)
//
// Build: tinyswift compile --no-prelude-import bugs/bug_106_critical_multiline_string.swift
// Run:   ./bug_106_critical_multiline_string

func main() -> Int {
  let s: String = """
  hello
  world
  """
  print(s)    // Prints '""' then '  hello' instead of 'hello\nworld'
  return 0
}
