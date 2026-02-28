// TinySwift Algorithmic Example: Stack and Queue
// Demonstrates: classes, optionals, generics, struct vs class, data structures.
// Build: tinyswift compile examples/stack_and_queue.swift
// Run:   ./stack_and_queue

// --- Stack node ---

class StackNode {
  var value: Int
  var next: StackNode?

  init(value: Int) {
    self.value = value
    self.next = nil
  }
}

// --- Stack (LIFO) ---

class Stack {
  var top: StackNode?
  var size: Int

  init() {
    self.top = nil
    self.size = 0
  }

  func push(_ value: Int) -> Void {
    let node: StackNode = StackNode(value: value)
    node.next = self.top
    self.top = node
    self.size = self.size + 1
  }

  func pop() -> Int? {
    switch self.top {
    case .none: return nil
    case .some(let node):
      self.top = node.next
      self.size = self.size - 1
      return node.value
    }
  }

  func peek() -> Int? {
    switch self.top {
    case .none: return nil
    case .some(let node): return node.value
    }
  }

  func isEmpty() -> Bool {
    return self.size == 0
  }

  func printAll() -> Void {
    var current: StackNode? = self.top
    while true {
      switch current {
      case .none: return
      case .some(let node):
        print(node.value)
        current = node.next
      }
    }
  }
}

// --- Queue node ---

class QueueNode {
  var value: Int
  var next: QueueNode?

  init(value: Int) {
    self.value = value
    self.next = nil
  }
}

// --- Queue (FIFO) ---

class Queue {
  var head: QueueNode?
  var tail: QueueNode?
  var size: Int

  init() {
    self.head = nil
    self.tail = nil
    self.size = 0
  }

  func enqueue(_ value: Int) -> Void {
    let node: QueueNode = QueueNode(value: value)
    switch self.tail {
    case .none:
      self.head = node
      self.tail = node
    case .some(let t):
      t.next = node
      self.tail = node
    }
    self.size = self.size + 1
  }

  func dequeue() -> Int? {
    switch self.head {
    case .none: return nil
    case .some(let node):
      self.head = node.next
      self.size = self.size - 1
      // If queue is now empty, clear tail
      switch self.head {
      case .none: self.tail = nil
      case .some: break
      }
      return node.value
    }
  }

  func front() -> Int? {
    switch self.head {
    case .none: return nil
    case .some(let node): return node.value
    }
  }

  func isEmpty() -> Bool {
    return self.size == 0
  }

  func printAll() -> Void {
    var current: QueueNode? = self.head
    while true {
      switch current {
      case .none: return
      case .some(let node):
        print(node.value)
        current = node.next
      }
    }
  }
}

// --- Use stack to check balanced brackets ---

func isBalanced(_ brackets: [Int]) -> Bool {
  // Encode: 1 = '(', 2 = ')', 3 = '[', 4 = ']', 5 = '{', 6 = '}'
  let stack: Stack = Stack()
  var i: Int = 0
  let size: Int = 12 // length of brackets array
  while i < size {
    let ch: Int = brackets[i]
    if ch == 1 || ch == 3 || ch == 5 {
      stack.push(ch)
    } else {
      if stack.isEmpty() {
        return false
      }
      let top: Int = stack.pop()!
      // Check matching: 1<->2, 3<->4, 5<->6
      if ch == 2 && top != 1 { return false }
      if ch == 4 && top != 3 { return false }
      if ch == 6 && top != 5 { return false }
    }
    i = i + 1
  }
  return stack.isEmpty()
}

// --- Use stack to reverse an array ---

func reverseArray(_ arr: [Int], _ size: Int) -> [Int] {
  let stack: Stack = Stack()
  var i: Int = 0
  while i < size {
    stack.push(arr[i])
    i = i + 1
  }

  var result: [Int] = []
  while !stack.isEmpty() {
    result = result + [stack.pop()!]
  }
  return result
}

// --- Entry point ---

func main() -> Int {
  // Stack demo
  print("Stack operations:")
  let stack: Stack = Stack()
  stack.push(10)
  stack.push(20)
  stack.push(30)

  print("Stack size:")
  print(stack.size)  // 3

  print("Peek:")
  print(stack.peek()!)  // 30

  print("Pop sequence:")
  print(stack.pop()!)  // 30
  print(stack.pop()!)  // 20
  print(stack.pop()!)  // 10

  print("Empty?")
  if stack.isEmpty() {
    print(1)
  } else {
    print(0)
  }

  // Queue demo
  print("Queue operations:")
  let queue: Queue = Queue()
  queue.enqueue(100)
  queue.enqueue(200)
  queue.enqueue(300)

  print("Queue size:")
  print(queue.size)  // 3

  print("Front:")
  print(queue.front()!)  // 100

  print("Dequeue sequence:")
  print(queue.dequeue()!)  // 100
  print(queue.dequeue()!)  // 200
  print(queue.dequeue()!)  // 300

  print("Empty?")
  if queue.isEmpty() {
    print(1)
  } else {
    print(0)
  }

  // Mixed operations
  print("Queue round-robin:")
  let q2: Queue = Queue()
  q2.enqueue(1)
  q2.enqueue(2)
  q2.enqueue(3)

  // Rotate: dequeue and re-enqueue
  var round: Int = 0
  while round < 3 {
    let val: Int = q2.dequeue()!
    print(val)
    q2.enqueue(val + 10)
    round = round + 1
  }

  print("Queue after rotation:")
  q2.printAll()  // 11 12 13

  return 0
}
