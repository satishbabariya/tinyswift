// BUG 6: [CRITICAL] Struct field mutation silently ignored
//
// Direct assignment to struct fields (`b.value = 99`) compiles but
// the value is never actually updated.
//
// EXPECTED: prints 99
// ACTUAL: prints 1 (original value)
//
// WORKAROUND: Create a new struct instance instead of mutating:
//   b = Box(value: 99)
//
// Build: tinyswift compile --no-prelude-import bugs/bug_06_critical_struct_field_mutation.swift
// Run:   ./bug_06_critical_struct_field_mutation

struct Box {
  var value: Int
}

func main() -> Int {
  var b: Box = Box(value: 1)
  b.value = 99
  print(b.value)  // Prints 1, not 99
  return 0
}
