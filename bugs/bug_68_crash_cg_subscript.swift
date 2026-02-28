// BUG 68: [CRASH-CG] Subscript declarations crash codegen
//
// Defining a subscript on a struct crashes the compiler with SIL error
// ("non-void function missing return") followed by LLVM assertion.
//
// EXPECTED: compiles and prints "1\n3\n5"
// ACTUAL: compiler crash (SIL + LLVM assertions)
//
// Build: tinyswift compile --no-prelude-import bugs/bug_68_crash_cg_subscript.swift

struct Grid {
  var data: [Int]
  var width: Int
  subscript(row: Int, col: Int) -> Int {   // COMPILER CRASH
    return data[row * width + col]
  }
}

func main() -> Int {
  let g: Grid = Grid(data: [1, 2, 3, 4, 5, 6], width: 3)
  print(g[0, 0])
  print(g[0, 2])
  print(g[1, 1])
  return 0
}
