// TinySwift Algorithmic Example: Linked List
// Demonstrates: classes, optionals, init/deinit, self, reference types, methods.
// Build: tinyswift compile examples/linked_list.swift
// Run:   ./linked_list

// --- Node class ---

class Node {
  var value: Int
  var next: Node?

  init(value: Int) {
    self.value = value
    self.next = nil
  }
}

// --- Singly Linked List ---

class LinkedList {
  var head: Node?
  var size: Int

  init() {
    self.head = nil
    self.size = 0
  }

  // Prepend to front: O(1)
  func prepend(_ value: Int) -> Void {
    let node: Node = Node(value: value)
    node.next = self.head
    self.head = node
    self.size = self.size + 1
  }

  // Check if empty
  func isEmpty() -> Bool {
    return self.size == 0
  }

  // Get the head value
  func first() -> Int? {
    switch self.head {
    case .none: return nil
    case .some(let node): return node.value
    }
  }

  // Check if a value exists
  func contains(_ target: Int) -> Bool {
    var current: Node? = self.head
    while true {
      switch current {
      case .none: return false
      case .some(let node):
        if node.value == target {
          return true
        }
        current = node.next
      }
    }
  }

  // Count occurrences
  func count(_ target: Int) -> Int {
    var result: Int = 0
    var current: Node? = self.head
    while true {
      switch current {
      case .none: return result
      case .some(let node):
        if node.value == target {
          result = result + 1
        }
        current = node.next
      }
    }
  }

  // Sum all values
  func sum() -> Int {
    var total: Int = 0
    var current: Node? = self.head
    while true {
      switch current {
      case .none: return total
      case .some(let node):
        total = total + node.value
        current = node.next
      }
    }
  }

  // Print all values
  func printAll() -> Void {
    var current: Node? = self.head
    while true {
      switch current {
      case .none: return
      case .some(let node):
        print(node.value)
        current = node.next
      }
    }
  }

  // Reverse the list in place
  func reverse() -> Void {
    var prev: Node? = nil
    var current: Node? = self.head
    while true {
      switch current {
      case .none:
        self.head = prev
        return
      case .some(let node):
        let nextNode: Node? = node.next
        node.next = prev
        prev = current
        current = nextNode
      }
    }
  }
}

// --- Entry point ---

func main() -> Int {
  let list: LinkedList = LinkedList()

  // Build the list: 5 -> 4 -> 3 -> 2 -> 1
  list.prepend(1)
  list.prepend(2)
  list.prepend(3)
  list.prepend(4)
  list.prepend(5)

  print("List contents:")
  list.printAll()

  print("Size:")
  print(list.size)

  print("Contains 3?")
  if list.contains(3) {
    print(1)
  } else {
    print(0)
  }

  print("Contains 9?")
  if list.contains(9) {
    print(1)
  } else {
    print(0)
  }

  print("Sum:")
  print(list.sum())

  // Reverse
  list.reverse()
  print("After reverse:")
  list.printAll()

  // Add duplicates
  list.prepend(3)
  list.prepend(3)
  print("Count of 3:")
  print(list.count(3))

  return 0
}
