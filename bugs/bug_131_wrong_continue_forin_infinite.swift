// BUG 131: [WRONG/CRASH] `continue` in for-in loop causes infinite loop or crash
//
// Using `continue` inside a for-in range loop either produces an infinite
// loop (the loop counter is not incremented) or crashes with SIL errors.
// `continue` works correctly inside while loops.
//
// EXPECTED: prints 1+2+4+5 = 12 (skipping 3)
// ACTUAL: infinite loop (timeout) or SIL terminator error
//
// WORKAROUND: Use while loop with manual counter, or restructure logic
// to avoid continue (use if-else instead).

func main() -> Int {
  var sum: Int = 0
  for i in 1...5 {
    if i == 3 { continue }   // INFINITE LOOP or SIL crash
    sum = sum + i
  }
  print(sum)
  return 0
}
