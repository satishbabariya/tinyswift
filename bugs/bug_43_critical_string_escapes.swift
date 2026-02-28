// BUG 43: [CRITICAL] String escape sequences not interpreted
//
// Escape sequences in string literals are printed as literal characters
// instead of being interpreted. Affects \t, \n, \\, and \".
//
// EXPECTED: actual tab, newline, backslash, and quote characters
// ACTUAL: literal \t, \n, \\, and \" printed as-is
//
// Build: tinyswift compile --no-prelude-import bugs/bug_43_critical_string_escapes.swift
// Run:   ./bug_43_critical_string_escapes

func main() -> Int {
  // Tab escape: should print "a<TAB>b"
  print("a\tb")      // ACTUAL: prints literal "a\tb"

  // Newline escape: should print "line1" then "line2"
  print("line1\nline2")  // ACTUAL: prints literal "line1\nline2"

  // Backslash escape: should print "a\b"
  print("a\\b")      // ACTUAL: prints literal "a\\b"

  // Quote escape: should print: he said "hi"
  print("he said \"hi\"")  // ACTUAL: prints literal: he said \"hi\"

  return 0
}
