// BUG 163: [SEMANTIC] Default field values not used in memberwise init
//
// Struct fields with default values (e.g., `var width: Int = 800`)
// are not used when calling the init without arguments. The compiler
// reports "missing field" even though a default is provided.
//
// EXPECTED: prints 800 then 600
// ACTUAL: error: missing field 'width' in struct initialization
//
// WORKAROUND: Always provide all fields in the init call, or write
// a custom init method.

struct Config {
  var width: Int = 800     // Default value ignored
  var height: Int = 600
}

func main() -> Int {
  let c: Config = Config()   // ERROR: missing field 'width'
  print(c.width)
  print(c.height)
  return 0
}
