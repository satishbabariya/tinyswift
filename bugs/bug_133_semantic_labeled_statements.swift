// BUG 133: [SEMANTIC] Labeled break/continue not parsed
//
// Statement labels (like `outer:` on loops) are not recognized by the
// parser, causing "use of undefined name" errors. This prevents breaking
// out of nested loops by label.
//
// EXPECTED: prints 304 (found at i=3, j=4)
// ACTUAL: error: use of undefined name 'outer'
//
// WORKAROUND: Use a flag variable and check it in the outer loop.

func main() -> Int {
  var found: Int = 0
  outer: for i in 1...5 {     // ERROR: 'outer' not recognized
    for j in 1...5 {
      if i * j == 12 {
        found = i * 100 + j
        break outer
      }
    }
  }
  print(found)
  return 0
}
