// BUG 96: [CRITICAL] Switch range patterns only match first value
//
// Range patterns in switch cases (e.g., `case 90...100:`) only match the
// first value in the range, not the full range. `case 90...100:` matches
// 90 but not 91-100.
//
// EXPECTED: grade(95) = "A", grade(85) = "B"
// ACTUAL: grade(95) = "F", grade(85) = "F" (only exact first value matches)
//
// Build: tinyswift compile --no-prelude-import bugs/bug_96_critical_switch_range_pattern.swift
// Run:   ./bug_96_critical_switch_range_pattern

func grade(_ score: Int) -> String {
  switch score {
  case 90...100: return "A"
  case 80...89: return "B"
  case 70...79: return "C"
  default: return "F"
  }
}

func main() -> Int {
  print(grade(90))    // "A" (matches — it's the first value)
  print(grade(95))    // "F" (WRONG: should be "A")
  print(grade(85))    // "F" (WRONG: should be "B")
  print(grade(50))    // "F" (correct)
  return 0
}
