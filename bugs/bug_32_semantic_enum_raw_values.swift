// BUG 32: [SEMANTIC] Enum raw values not accessible
//
// `.rawValue` property on raw-value enums is not recognized.
//
// EXPECTED: prints 3
// ACTUAL: ERROR: no member 'rawValue'
//
// Build: tinyswift compile --no-prelude-import bugs/bug_32_semantic_enum_raw_values.swift

enum Planet: Int {
  case mercury = 1
  case venus = 2
  case earth = 3
}

func main() -> Int {
  print(Planet.earth.rawValue)  // ERROR: no member 'rawValue'
  return 0
}
