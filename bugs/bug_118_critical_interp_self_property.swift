// BUG 118: [CRITICAL] String interpolation with self.property is empty
//
// Using `\(self.property)` in string interpolation inside struct or
// extension methods produces an empty string. This is a specific
// manifestation of Bug 44 (expression interpolation) since self.property
// is a member access expression, not a simple variable reference.
//
// EXPECTED: prints "ID-42"
// ACTUAL: prints "ID-" (self.num interpolates to empty)
//
// WORKAROUND: Copy self.property to a local variable first:
//   let n: Int = self.num
//   return "ID-\(n)"
//
// Build: tinyswift compile --no-prelude-import --output=OUT bugs/bug_118_critical_interp_self_property.swift

struct ID {
  var num: Int
  func display() -> String {
    return "ID-\(self.num)"   // Produces "ID-" (empty)
  }
}

// Also fails in extensions:
// extension ID {
//   func show() -> String { return "ID-\(self.num)" }  // empty
// }

func main() -> Int {
  let id: ID = ID(num: 42)
  print(id.display())
  return 0
}
