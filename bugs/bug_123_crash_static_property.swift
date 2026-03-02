// BUG 123: [CRASH] Static properties crash codegen
//
// Static let properties on structs cause debug info metadata errors.
// Note: static METHODS work correctly.
//
// EXPECTED: compiles and prints 5
// ACTUAL: !dbg attachment points at wrong subprogram for function
//
// WORKAROUND: Use a static method returning the value, or a free constant.

struct Config {
  static let maxRetries: Int = 5   // CRASH
}

func main() -> Int {
  print(Config.maxRetries)
  return 0
}
