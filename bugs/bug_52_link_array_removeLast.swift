// BUG 52: [LINK] Array.removeLast() generates undefined symbol
//
// Calling .removeLast() on a var array compiles to codegen but produces
// an undefined reference at link time (<unknown_callee>).
//
// EXPECTED: compiles and prints 2
// ACTUAL: linker error: undefined reference to `<unknown_callee>`
//
// Build: tinyswift compile --no-prelude-import bugs/bug_52_link_array_removeLast.swift

func main() -> Int {
  var arr: [Int] = [1, 2, 3]
  arr.removeLast()    // LINK ERROR
  print(arr.count)
  return 0
}
