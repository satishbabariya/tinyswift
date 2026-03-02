// BUG 136: [CRASH] Subscript declarations crash codegen
//
// Custom subscript declarations on structs cause "non-void function
// missing return" and InsertValueInst assertion failures.
//
// EXPECTED: prints 10 then 30
// ACTUAL: InsertValueInst assertion failure
//
// WORKAROUND: Use a regular method (e.g., func get(_ i: Int) -> Int).

struct Grid {
  var data: [Int]
  subscript(index: Int) -> Int {   // CRASH
    return data[index]
  }
}

func main() -> Int {
  let g: Grid = Grid(data: [10, 20, 30])
  print(g[0])
  print(g[2])
  return 0
}
