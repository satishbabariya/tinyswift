// BUG 102: [CRASH-CG] Custom initializers crash codegen
//
// Defining a custom `init` on a struct crashes the compiler. Two forms:
// 1. Custom init with params: LLVM CallInst function signature crash
// 2. Default init (no params, assigns fields): "uninitialized variable" error
//
// The auto-generated memberwise initializer works fine.
//
// EXPECTED: compiles and prints "3\n4"
// ACTUAL: compiler crash
//
// Build: tinyswift compile --no-prelude-import bugs/bug_102_crash_cg_custom_init.swift

struct Point {
  var x: Int
  var y: Int
  init(_ x: Int, _ y: Int) {      // COMPILER CRASH
    self.x = x
    self.y = y
  }
}

func main() -> Int {
  let p: Point = Point(3, 4)
  print(p.x)
  print(p.y)
  return 0
}
