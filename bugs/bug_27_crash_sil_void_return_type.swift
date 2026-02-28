// BUG 27: [CRASH-SIL] -> Void return type causes SIL errors
//
// Explicit `-> Void` return type annotation causes SIL verification
// errors: "non-void function missing return on all paths".
//
// EXPECTED: compiles and prints 42
// ACTUAL: SIL error during compilation
//
// WORKAROUND: Omit the return type annotation entirely:
//   func doSomething() { print(42); return }
//
// Build: tinyswift compile --no-prelude-import bugs/bug_27_crash_sil_void_return_type.swift

func doSomething() -> Void {
  print(42)
}

func main() -> Int {
  doSomething()
  return 0
}
