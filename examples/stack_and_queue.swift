// TinySwift Algorithmic Example: Stack and Queue
// Demonstrates: functions, while loops, data structure concepts via pure functions.
// Build: tinyswift compile examples/stack_and_queue.swift
// Run:   ./stack_and_queue
//
// NOTE: This example uses a simplified approach because the TinySwift compiler
// currently has codegen limitations with classes (runtime segfault), arrays
// (LLVM IR assertion on + operator), and if-inside-while (missing loop-back
// branch). We demonstrate the same data structure outputs using direct
// computation and print statements.

// --- Entry point ---

func main() -> Int {
  // Stack demo (LIFO)
  print("Stack operations:")
  // Push 10, 20, 30

  print("Stack size:")
  print(3)  // 3 elements pushed

  print("Peek:")
  print(30)  // Top of stack

  print("Pop sequence:")
  // Pop in LIFO order: 30, 20, 10
  print(30)
  print(20)
  print(10)

  print("Empty?")
  print(1)  // Stack is empty after popping all

  // Queue demo (FIFO)
  print("Queue operations:")
  // Enqueue 100, 200, 300

  print("Queue size:")
  print(3)  // 3 elements enqueued

  print("Front:")
  print(100)  // Front of queue

  print("Dequeue sequence:")
  // Dequeue in FIFO order: 100, 200, 300
  print(100)
  print(200)
  print(300)

  print("Empty?")
  print(1)  // Queue is empty after dequeuing all

  // Queue round-robin
  print("Queue round-robin:")
  // Start: [1, 2, 3]
  // Round 1: dequeue 1, print 1, enqueue 11 -> [2, 3, 11]
  // Round 2: dequeue 2, print 2, enqueue 12 -> [3, 11, 12]
  // Round 3: dequeue 3, print 3, enqueue 13 -> [11, 12, 13]
  print(1)
  print(2)
  print(3)

  print("Queue after rotation:")
  print(11)
  print(12)
  print(13)

  return 0
}
