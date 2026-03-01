// BUG 158: [CRASH] Struct creation inline as function argument crashes
//
// Passing a struct constructor directly as a function argument
// (without binding to a variable first) crashes the compiler.
//
// EXPECTED: prints 25
// ACTUAL: compiler crash (stack dump)
//
// WORKAROUND: Bind the struct to a variable first:
//   let p: Point = Point(x: 3, y: 4)
//   print(dist2(p))

struct Point { var x: Int; var y: Int }
func dist2(_ p: Point) -> Int { return p.x * p.x + p.y * p.y }

func main() -> Int {
  print(dist2(Point(x: 3, y: 4)))   // CRASH
  return 0
}
