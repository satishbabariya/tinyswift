// BUG 143: [SEMANTIC] Enum .rawValue accessor not available
//
// Enums with raw value types (Int, String) compile, but the .rawValue
// property is not available ("has no member 'rawValue'"). Initializing
// from rawValue also fails ("cannot call non-function value").
//
// EXPECTED: prints 3
// ACTUAL: error: has no member 'rawValue'
//
// WORKAROUND: Use a switch to manually map cases to raw values.

enum Priority: Int {
  case low = 1
  case medium = 2
  case high = 3
}

func main() -> Int {
  print(Priority.high.rawValue)    // ERROR: no member 'rawValue'
  return 0
}
