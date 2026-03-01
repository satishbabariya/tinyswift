// BUG 139: [WRONG] Overflow operators (&+, &-, &*) don't overflow
//
// The overflow addition operator &+ returns the original value instead
// of wrapping around. Int.max &+ 1 should give Int.min.
//
// EXPECTED: -9223372036854775808
// ACTUAL: 9223372036854775807 (original value, no overflow)
//
// WORKAROUND: Use regular + for wrapping (it wraps silently in tinyswift).

func main() -> Int {
  let max: Int = 9223372036854775807
  let wrapped: Int = max &+ 1
  print(wrapped)   // Prints max instead of min
  return 0
}
