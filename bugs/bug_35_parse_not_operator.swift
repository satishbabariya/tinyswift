// BUG 35: [PARSE] ! prefix operator not supported
//
// `!condition` is always tokenized as force unwrap (Exclaim), not
// as a prefix NOT operator.
//
// EXPECTED: compiles and prints 1
// ACTUAL: Parser crash
//
// WORKAROUND: Use `flag == false` instead.
//
// Build: tinyswift compile --no-prelude-import bugs/bug_35_parse_not_operator.swift

func main() -> Int {
  let flag: Bool = false
  if !flag {
    print(1)
  }
  return 0
}
