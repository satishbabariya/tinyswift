// BUG 37: [PARSE] Semicolons as statement separators in structs
//
// Semicolons between declarations in struct bodies crash parser.
//
// EXPECTED: compiles and runs
// ACTUAL: Parser crash
//
// WORKAROUND: Use newlines to separate declarations.
//
// Build: tinyswift compile --no-prelude-import bugs/bug_37_parse_semicolons_in_struct.swift

struct Pair { var x: Int; var y: Int }

func main() -> Int {
  let p: Pair = Pair(x: 1, y: 2)
  print(p.x)
  return 0
}
