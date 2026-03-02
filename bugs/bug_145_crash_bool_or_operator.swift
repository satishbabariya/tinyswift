// BUG 145: [CRASH] Boolean || operator crashes at runtime
//
// The || (logical OR) operator compiles but causes a segfault at runtime.
//
// EXPECTED: prints "either"
// ACTUAL: segfault (rc=139)
//
// WORKAROUND: Use sequential if-statements:
//   if a { print("either") }
//   if b { print("either") }

func main() -> Int {
  let a: Bool = false
  let b: Bool = true
  if a || b { print("either") }   // SEGFAULT
  return 0
}
