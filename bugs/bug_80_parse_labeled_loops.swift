// BUG 80: [PARSE] Labeled loops not supported
//
// Swift's labeled loop syntax (`label: for ...`) is parsed as an
// undefined name reference instead of a loop label.
//
// EXPECTED: compiles and breaks out of nested loop
// ACTUAL: error: use of undefined name 'outer'
//
// Build: tinyswift compile --no-prelude-import bugs/bug_80_parse_labeled_loops.swift

func main() -> Int {
  var found: Int = 0
  outer: for i in 1...5 {     // ERROR: use of undefined name 'outer'
    for j in 1...5 {
      if i * j > 10 {
        found = i * j
        break outer
      }
    }
  }
  print(found)
  return 0
}
