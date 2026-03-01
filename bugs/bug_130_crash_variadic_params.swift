// BUG 130: [CRASH] Variadic parameters crash codegen
//
// Functions with variadic parameters (Int...) cause a stack dump crash
// during compilation.
//
// EXPECTED: prints 15
// ACTUAL: compiler crash (stack dump)
//
// WORKAROUND: Accept an array parameter instead of variadic.

func sum(_ values: Int...) -> Int {   // CRASH
  var total: Int = 0
  for v in values {
    total = total + v
  }
  return total
}

func main() -> Int {
  print(sum(1, 2, 3, 4, 5))
  return 0
}
