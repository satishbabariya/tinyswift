// BUG 109: [PARSE] Qualified enum case name in switch crashes parser
//
// Using fully-qualified enum case names (e.g., `case Color.red:`) in
// switch case patterns crashes the parser with "Expected :, got .".
// The parser sees `case Color` and expects `:` but finds `.red`.
//
// Using implicit member syntax (`case .red:`) works correctly.
//
// EXPECTED: compiles and prints "RED"
// ACTUAL: parser crash (CHECK failure at parse/context.cpp:48)
//
// WORKAROUND: Use dot syntax (`.caseName`) in switch case patterns.
//
// Build: tinyswift compile --no-prelude-import --output=OUT bugs/bug_109_parse_qualified_enum_in_switch.swift

enum Color {
  case red
  case blue
}

func describe(_ c: Color) -> String {
  switch c {
  case Color.red: return "RED"     // PARSER CRASH: "Expected :, got ."
  case Color.blue: return "BLUE"
  }
}

// WORKAROUND (this compiles):
//   switch c {
//   case .red: return "RED"
//   case .blue: return "BLUE"
//   }

func main() -> Int {
  print(describe(Color.red))
  return 0
}
