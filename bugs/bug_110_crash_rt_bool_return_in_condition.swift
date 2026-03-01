// BUG 110: [CRASH-RT] Bool returned from function segfaults in conditions
//
// When a function returns Bool, using the returned value in an
// if/while condition segfaults at runtime. Storing and printing the
// Bool works (prints -1 per Bug 38), but using it as a condition crashes.
//
// Passing Bool as a parameter to a function and using it in `if b`
// works fine. The crash is specific to Bool VALUES RETURNED from functions.
//
// EXPECTED: prints "positive"
// ACTUAL: segfault (signal 11)
//
// WORKAROUND: Return Int (0/1) instead of Bool, then compare with == 1.
//
// Build: tinyswift compile --no-prelude-import --output=OUT bugs/bug_110_crash_rt_bool_return_in_condition.swift

func isPositive(_ x: Int) -> Bool {
  if x > 0 { return true }
  return false
}

func main() -> Int {
  // All three forms segfault:

  // Form 1: direct call in if
  if isPositive(5) {        // SEGFAULT
    print("positive")
  }

  // Form 2: stored then used in if
  // let r: Bool = isPositive(5)
  // if r { print("positive") }   // SEGFAULT

  // Form 3: stored then compared
  // let r: Bool = isPositive(5)
  // if r == true { print("positive") }  // SEGFAULT

  return 0
}
