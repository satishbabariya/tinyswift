// TinySwift Algorithmic Example: Sorting Algorithms
// Demonstrates: arrays, loops, inout, swap, comparisons.
// Build: tinyswift compile examples/sorting.swift
// Run:   ./sorting

// --- Helper: swap two elements ---

func swapInts(_ a: inout Int, _ b: inout Int) -> Void {
  let temp: Int = a
  a = b
  b = temp
}

// --- Bubble Sort ---

func bubbleSort(_ arr: inout [Int], _ size: Int) -> Void {
  var i: Int = 0
  while i < size - 1 {
    var j: Int = 0
    while j < size - 1 - i {
      if arr[j] > arr[j + 1] {
        let temp: Int = arr[j]
        arr[j] = arr[j + 1]
        arr[j + 1] = temp
      }
      j = j + 1
    }
    i = i + 1
  }
}

// --- Selection Sort ---

func selectionSort(_ arr: inout [Int], _ size: Int) -> Void {
  var i: Int = 0
  while i < size - 1 {
    var minIdx: Int = i
    var j: Int = i + 1
    while j < size {
      if arr[j] < arr[minIdx] {
        minIdx = j
      }
      j = j + 1
    }
    if minIdx != i {
      let temp: Int = arr[i]
      arr[i] = arr[minIdx]
      arr[minIdx] = temp
    }
    i = i + 1
  }
}

// --- Insertion Sort ---

func insertionSort(_ arr: inout [Int], _ size: Int) -> Void {
  var i: Int = 1
  while i < size {
    let key: Int = arr[i]
    var j: Int = i - 1
    while j >= 0 && arr[j] > key {
      arr[j + 1] = arr[j]
      j = j - 1
    }
    arr[j + 1] = key
    i = i + 1
  }
}

// --- Check if sorted ---

func isSorted(_ arr: [Int], _ size: Int) -> Bool {
  var i: Int = 0
  while i < size - 1 {
    if arr[i] > arr[i + 1] {
      return false
    }
    i = i + 1
  }
  return true
}

// --- Print array ---

func printArray(_ arr: [Int], _ size: Int) -> Void {
  var i: Int = 0
  while i < size {
    print(arr[i])
    i = i + 1
  }
}

// --- Find min and max ---

func findMin(_ arr: [Int], _ size: Int) -> Int {
  var result: Int = arr[0]
  var i: Int = 1
  while i < size {
    if arr[i] < result {
      result = arr[i]
    }
    i = i + 1
  }
  return result
}

func findMax(_ arr: [Int], _ size: Int) -> Int {
  var result: Int = arr[0]
  var i: Int = 1
  while i < size {
    if arr[i] > result {
      result = arr[i]
    }
    i = i + 1
  }
  return result
}

// --- Entry point ---

func main() -> Int {
  // Test bubble sort
  print("Bubble sort:")
  var arr1: [Int] = [64, 34, 25, 12, 22, 11, 90]
  let size1: Int = 7
  bubbleSort(&arr1, size1)
  printArray(arr1, size1)

  print("Sorted?")
  if isSorted(arr1, size1) {
    print(1)
  } else {
    print(0)
  }

  // Test selection sort
  print("Selection sort:")
  var arr2: [Int] = [29, 10, 14, 37, 13]
  let size2: Int = 5
  selectionSort(&arr2, size2)
  printArray(arr2, size2)

  // Test insertion sort
  print("Insertion sort:")
  var arr3: [Int] = [5, 2, 4, 6, 1, 3]
  let size3: Int = 6
  insertionSort(&arr3, size3)
  printArray(arr3, size3)

  // Find min/max
  print("Min:")
  print(findMin(arr3, size3))
  print("Max:")
  print(findMax(arr3, size3))

  return 0
}
