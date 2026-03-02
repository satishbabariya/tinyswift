// BUG 161: [CRASH] Array literals with 22+ non-uniform elements crash
//
// Array literals with more than 21 elements crash during compilation
// when they contain non-uniform values (e.g., [0,1,0,0,...]).
// Arrays with all-same values (e.g., [0,0,0,...]) work up to 50+.
// Arrays with 21 or fewer non-uniform elements work.
//
// EXPECTED: prints 10946
// ACTUAL: compiler crash (stack dump)
//
// WORKAROUND: Initialize with all-zeros and assign non-zero values
// after initialization:
//   var arr: [Int] = [0,0,0,0,...0]  // all zeros, any size
//   arr[1] = 1                       // set non-zero values after

func main() -> Int {
  var fib: [Int] = [0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0]  // 22 elements: CRASH
  for i in 2...21 {
    fib[i] = fib[i-1] + fib[i-2]
  }
  print(fib[21])
  return 0
}
