// BUG 119: [SEMANTIC] Protocol extension default implementations not available
//
// Default method implementations provided via protocol extensions are
// not picked up by conforming types. The compiler reports "has no member"
// even though the protocol extension defines the method.
//
// Additionally, protocol extensions cannot access protocol requirements
// via `self` - "has no member" for protocol-required properties.
//
// EXPECTED: prints "hello"
// ACTUAL: error: value of type 'Bot' has no member 'greet'
//
// WORKAROUND: Implement all protocol methods directly in each
// conforming type (no default implementations).
//
// Build: tinyswift compile --no-prelude-import --output=OUT bugs/bug_119_semantic_protocol_default_impl.swift

protocol Greetable {
  func greet() -> String
}

extension Greetable {
  func greet() -> String { return "hello" }   // Default implementation
}

struct Bot: Greetable {}   // Should inherit default greet()

func main() -> Int {
  let b: Bot = Bot()
  print(b.greet())   // ERROR: has no member 'greet'
  return 0
}
