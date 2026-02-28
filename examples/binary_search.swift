// TinySwift Algorithmic Example: Binary Search
// Demonstrates: arrays, while loops, comparisons, optionals, divide and conquer.
// Build: tinyswift compile examples/binary_search.swift
// Run:   ./binary_search

// --- Iterative binary search ---

func binarySearch(_ arr: [Int], _ size: Int, _ target: Int) -> Int {
  var lo: Int = 0
  var hi: Int = size - 1
  while lo <= hi {
    let mid: Int = lo + (hi - lo) / 2
    if arr[mid] == target {
      return mid
    }
    if arr[mid] < target {
      lo = mid + 1
    } else {
      hi = mid - 1
    }
  }
  return -1  // not found
}

// --- Recursive binary search ---

func binarySearchRecursive(_ arr: [Int], _ target: Int, _ lo: Int, _ hi: Int) -> Int {
  if lo > hi {
    return -1
  }
  let mid: Int = lo + (hi - lo) / 2
  if arr[mid] == target {
    return mid
  }
  if arr[mid] < target {
    return binarySearchRecursive(arr, target, mid + 1, hi)
  }
  return binarySearchRecursive(arr, target, lo, mid - 1)
}

// --- Find first occurrence (lower bound) ---

func lowerBound(_ arr: [Int], _ size: Int, _ target: Int) -> Int {
  var lo: Int = 0
  var hi: Int = size
  while lo < hi {
    let mid: Int = lo + (hi - lo) / 2
    if arr[mid] < target {
      lo = mid + 1
    } else {
      hi = mid
    }
  }
  return lo
}

// --- Find last occurrence (upper bound) ---

func upperBound(_ arr: [Int], _ size: Int, _ target: Int) -> Int {
  var lo: Int = 0
  var hi: Int = size
  while lo < hi {
    let mid: Int = lo + (hi - lo) / 2
    if arr[mid] <= target {
      lo = mid + 1
    } else {
      hi = mid
    }
  }
  return lo
}

// --- Count occurrences using bounds ---

func countOccurrences(_ arr: [Int], _ size: Int, _ target: Int) -> Int {
  let lo: Int = lowerBound(arr, size, target)
  let hi: Int = upperBound(arr, size, target)
  return hi - lo
}

// --- Find insertion point (maintain sorted order) ---

func insertionPoint(_ arr: [Int], _ size: Int, _ target: Int) -> Int {
  return lowerBound(arr, size, target)
}

// --- Binary search on answer: find integer square root ---

func intSqrt(_ n: Int) -> Int {
  if n < 0 {
    return -1
  }
  if n == 0 {
    return 0
  }
  var lo: Int = 1
  var hi: Int = n
  var result: Int = 0
  while lo <= hi {
    let mid: Int = lo + (hi - lo) / 2
    if mid <= n / mid {
      result = mid
      lo = mid + 1
    } else {
      hi = mid - 1
    }
  }
  return result
}

// --- Entry point ---

func main() -> Int {
  let arr: [Int] = [2, 5, 8, 12, 16, 23, 38, 45, 56, 72, 91]
  let size: Int = 11

  // Basic search
  print("Searching for 23:")
  let idx: Int = binarySearch(arr, size, 23)
  print(idx)  // expected: 5

  print("Searching for 50:")
  let notFound: Int = binarySearch(arr, size, 50)
  print(notFound)  // expected: -1

  // Recursive search
  print("Recursive search for 56:")
  let ridx: Int = binarySearchRecursive(arr, 56, 0, size - 1)
  print(ridx)  // expected: 8

  // Duplicates array
  let dups: [Int] = [1, 2, 2, 2, 3, 3, 5, 5, 5, 5, 7]
  let dupSize: Int = 11

  print("Count of 2:")
  print(countOccurrences(dups, dupSize, 2))  // expected: 3

  print("Count of 5:")
  print(countOccurrences(dups, dupSize, 5))  // expected: 4

  print("Count of 4:")
  print(countOccurrences(dups, dupSize, 4))  // expected: 0

  // Lower and upper bounds
  print("Lower bound of 3:")
  print(lowerBound(dups, dupSize, 3))  // expected: 4

  print("Upper bound of 3:")
  print(upperBound(dups, dupSize, 3))  // expected: 6

  // Integer square root
  print("intSqrt(0):")
  print(intSqrt(0))   // 0

  print("intSqrt(1):")
  print(intSqrt(1))   // 1

  print("intSqrt(16):")
  print(intSqrt(16))  // 4

  print("intSqrt(26):")
  print(intSqrt(26))  // 5

  print("intSqrt(100):")
  print(intSqrt(100)) // 10

  return 0
}
