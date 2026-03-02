// BUG 49: [SEMANTIC] Extension methods can't access struct fields without self. prefix
//
// In extension methods on structs, accessing fields without the `self.`
// prefix produces "use of undefined name" error. Struct's own methods
// (defined in the struct body) can access fields without self. prefix,
// but extension methods cannot.
//
// EXPECTED: compiles and prints 10
// ACTUAL: error: use of undefined name 'value'
//
// WORKAROUND: Use `self.fieldName` explicitly in extension methods.
//
// Build: tinyswift compile --no-prelude-import bugs/bug_49_semantic_extension_implicit_self.swift

struct Box {
  var value: Int

  // This works - struct's own method accesses field without self:
  func getVal() -> Int {
    return value
  }
}

extension Box {
  // This fails - extension method can't access field without self:
  func doubled() -> Int {
    return value * 2  // ERROR: use of undefined name 'value'
  }

  // Workaround - using self. prefix works:
  // func doubled() -> Int {
  //   return self.value * 2  // OK
  // }
}

func main() -> Int {
  let b: Box = Box(value: 5)
  print(b.doubled())
  return 0
}
