// BUG 7: [CRITICAL] Tuple element access always returns first element
//
// Accessing .1, .2, etc. on a tuple always returns the value of .0.
//
// EXPECTED: prints 10 then 20
// ACTUAL: prints 10 then 10 (both return first element)
//
// Build: tinyswift compile --no-prelude-import bugs/bug_07_critical_tuple_element_access.swift
// Run:   ./bug_07_critical_tuple_element_access

func makePair(_ a: Int, _ b: Int) -> (Int, Int) {
  return (a, b)
}

func main() -> Int {
  let t: (Int, Int) = makePair(10, 20)
  print(t.0)  // 10 (correct)
  print(t.1)  // 10 (WRONG - should be 20)
  return 0
}
