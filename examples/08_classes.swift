// TinySwift Example: Classes
// Demonstrates class definitions, initializers, deinitializers, and reference semantics.

// --- Basic class with init ---

class Dog {
  var name: String
  var age: Int

  init(name: String, age: Int) {
    self.name = name
    self.age = age
  }

  func bark() -> String {
    return "Woof!"
  }

  func description() -> String {
    return name
  }
}

// Usage: let d = Dog(name: "Rex", age: 3)

// --- Class with deinit ---

class Resource {
  var id: Int
  var active: Bool

  init(id: Int) {
    self.id = id
    self.active = true
  }

  deinit {
    // Cleanup when instance is deallocated
  }

  func deactivate() -> Void {
    self.active = false
  }
}

// --- Class with multiple initializers ---

class Vector {
  var x: Double
  var y: Double

  init(x: Double, y: Double) {
    self.x = x
    self.y = y
  }

  func magnitude() -> Double {
    return sqrt(x * x + y * y)
  }

  func add(_ other: Vector) -> Vector {
    return Vector(x: x + other.x, y: y + other.y)
  }

  func scale(_ factor: Double) -> Vector {
    return Vector(x: x * factor, y: y * factor)
  }

  func dot(_ other: Vector) -> Double {
    return x * other.x + y * other.y
  }
}

// --- Class with self references ---

class Node {
  var value: Int
  var next: Node?

  init(value: Int) {
    self.value = value
    self.next = nil
  }

  func hasNext() -> Bool {
    switch self.next {
    case .none: return false
    case .some: return true
    }
  }
}

// --- Class with private state ---

class BankAccount {
  private var balance: Int

  init(initialBalance: Int) {
    self.balance = initialBalance
  }

  public func getBalance() -> Int {
    return balance
  }

  public func deposit(_ amount: Int) -> Void {
    balance = balance + amount
  }

  public func withdraw(_ amount: Int) -> Bool {
    if amount > balance {
      return false
    }
    balance = balance - amount
    return true
  }
}

// --- Class used as reference type ---

class SharedCounter {
  var value: Int

  init() {
    self.value = 0
  }

  func increment() -> Void {
    self.value = self.value + 1
  }

  func getValue() -> Int {
    return self.value
  }
}
