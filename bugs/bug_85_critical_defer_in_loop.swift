// BUG 85: [CRITICAL] defer in loop body doesn't execute per-iteration
//
// defer blocks inside loop bodies should execute at the end of each
// iteration. Instead, they appear to be ignored entirely inside loops.
//
// defer at function scope works correctly (Bug-free for that case).
//
// EXPECTED: "0\ndefer\n1\ndefer\n2\ndefer"
// ACTUAL: "0\n1\n2" (defer never runs)
//
// Build: tinyswift compile --no-prelude-import bugs/bug_85_critical_defer_in_loop.swift
// Run:   ./bug_85_critical_defer_in_loop

func main() -> Int {
  var i: Int = 0
  while i < 3 {
    defer { print("defer") }    // Never executes
    print(i)
    i = i + 1
  }
  return 0
}
