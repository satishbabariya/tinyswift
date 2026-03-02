// BUG 11: [CRASH-RT] Classes segfault at runtime
//
// Any class instantiation segfaults. Struct equivalent works fine.
//
// EXPECTED: prints 42
// ACTUAL: compiles but segfaults on class instantiation
//
// WORKAROUND: Use structs instead of classes.
//
// Build: tinyswift compile --no-prelude-import bugs/bug_11_crash_rt_classes.swift
// Run:   ./bug_11_crash_rt_classes

class Box {
  var value: Int
  init(value: Int) { self.value = value }
}

func main() -> Int {
  let b: Box = Box(value: 42)
  print(b.value)  // SEGFAULT
  return 0
}
