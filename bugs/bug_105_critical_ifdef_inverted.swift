// BUG 105: [CRITICAL] #if/#else/#endif conditional compilation inverted
//
// `#if true` takes the `#else` branch instead of the `#if` branch.
// The condition evaluation is inverted.
//
// EXPECTED: prints "yes" (#if true branch)
// ACTUAL: prints "no" (#else branch)
//
// Build: tinyswift compile --no-prelude-import bugs/bug_105_critical_ifdef_inverted.swift
// Run:   ./bug_105_critical_ifdef_inverted

func main() -> Int {
  #if true
  print("yes")     // Should be compiled
  #else
  print("no")      // Should be skipped
  #endif
  return 0
}
