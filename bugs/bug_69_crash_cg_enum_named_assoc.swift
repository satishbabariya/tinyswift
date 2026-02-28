// BUG 69: [CRASH-CG] Enum cases with named/multiple associated values crash
//
// Enum cases with named associated values (e.g., `case rect(width: Int, height: Int)`)
// or with 3+ positional associated values crash the compiler during codegen.
//
// Enum cases with a single unnamed associated value work fine.
//
// EXPECTED: compiles and prints "0\n75\n12"
// ACTUAL: compiler crash
//
// Build: tinyswift compile --no-prelude-import bugs/bug_69_crash_cg_enum_named_assoc.swift

enum Shape {
  case point
  case circle(radius: Int)               // Named assoc value
  case rect(width: Int, height: Int)     // Multiple named assoc values
}

func area(_ s: Shape) -> Int {
  switch s {
  case .point: return 0
  case .circle(let r): return r * r * 3
  case .rect(let w, let h): return w * h
  }
}

func main() -> Int {
  print(area(Shape.point))
  print(area(Shape.circle(radius: 5)))
  print(area(Shape.rect(width: 3, height: 4)))
  return 0
}
