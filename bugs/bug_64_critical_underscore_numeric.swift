// BUG 64: [CRITICAL] Underscore separators in numeric literals produce wrong value
//
// Swift allows underscores in numeric literals for readability
// (e.g., 1_000_000). TinySwift parses these but produces the wrong
// value: 1_000_000 evaluates to 0 instead of 1000000.
//
// EXPECTED: prints 1000000
// ACTUAL: prints 0
//
// Build: tinyswift compile --no-prelude-import bugs/bug_64_critical_underscore_numeric.swift
// Run:   ./bug_64_critical_underscore_numeric

func main() -> Int {
  let x: Int = 1_000_000
  print(x)            // Prints 0, should print 1000000

  let y: Int = 1000000
  print(y)            // Prints 1000000 (no underscores works)

  return 0
}
