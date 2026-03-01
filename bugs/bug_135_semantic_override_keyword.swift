// BUG 135: [SEMANTIC] `override` keyword not parsed
//
// The `override` keyword on methods in subclasses is not recognized,
// causing "expected declaration" parse errors. This blocks class
// inheritance with method overriding.
//
// EXPECTED: compiles and prints "meow"
// ACTUAL: error: expected declaration (on override keyword)
//
// WORKAROUND: None for class inheritance. Use protocol conformance
// with structs as an alternative design pattern.

class Animal {
  var name: String
  init(name: String) { self.name = name }
  func sound() -> String { return "..." }
}

class Cat: Animal {
  override func sound() -> String { return "meow" }  // ERROR
}

func main() -> Int {
  let c: Cat = Cat(name: "Luna")
  print(c.sound())
  return 0
}
