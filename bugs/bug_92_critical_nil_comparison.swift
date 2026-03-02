// BUG 92: [CRITICAL] nil comparison (== nil, != nil) silently fails
//
// Comparing an optional to nil (`x == nil` or `x != nil`) compiles
// but the comparison always evaluates wrong — no output is produced
// and the process exits with code 48.
//
// EXPECTED: prints "nil"
// ACTUAL: no output, exit code 48
//
// WORKAROUND: Use if-let or switch with optional pattern matching.
//
// Build: tinyswift compile --no-prelude-import bugs/bug_92_critical_nil_comparison.swift
// Run:   ./bug_92_critical_nil_comparison

func main() -> Int {
  let x: Int? = nil
  if x == nil {           // Fails silently
    print("nil")
  }
  return 0
}

// Also affected:
// let y: Int? = 42
// if y != nil { print("not nil") }  // Also fails
