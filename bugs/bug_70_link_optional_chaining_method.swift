// BUG 70: [LINK] Optional chaining method call generates undefined reference
//
// Using optional chaining syntax `value?.method()` compiles through
// codegen but produces an undefined reference at link time.
//
// EXPECTED: compiles and prints 10
// ACTUAL: linker error: undefined reference to `<unknown_callee>`
//
// Build: tinyswift compile --no-prelude-import bugs/bug_70_link_optional_chaining_method.swift

struct Box {
  var val: Int
  func doubled() -> Int {
    return val * 2
  }
}

func main() -> Int {
  let b: Box? = Box(val: 5)
  let r: Int? = b?.doubled()    // LINK ERROR
  print(r ?? 0)
  return 0
}
