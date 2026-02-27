// TinySwift Example: Generics
// Demonstrates generic functions, generic types, and type constraints.

// --- Generic function ---

func identity<T>(_ value: T) -> T {
  return value
}

// --- Generic function with multiple type parameters ---

func makePair<A, B>(_ first: A, _ second: B) -> (A, B) {
  return (first, second)
}

// --- Generic struct ---

struct Box<T> {
  var value: T

  func get() -> T {
    return value
  }

  func map<U>(_ transform: (T) -> U) -> Box<U> {
    return Box<U>(value: transform(value))
  }
}

// --- Generic struct with multiple type parameters ---

struct Pair<First, Second> {
  var first: First
  var second: Second

  func getFirst() -> First {
    return first
  }

  func getSecond() -> Second {
    return second
  }

  func mapFirst<NewFirst>(_ transform: (First) -> NewFirst) -> Pair<NewFirst, Second> {
    return Pair<NewFirst, Second>(first: transform(first), second: second)
  }

  func mapSecond<NewSecond>(_ transform: (Second) -> NewSecond) -> Pair<First, NewSecond> {
    return Pair<First, NewSecond>(first: first, second: transform(second))
  }
}

// --- Generic enum ---

enum Either<Left, Right> {
  case left(Left)
  case right(Right)

  func isLeft() -> Bool {
    switch self {
    case .left: return true
    case .right: return false
    }
  }

  func isRight() -> Bool {
    switch self {
    case .left: return false
    case .right: return true
    }
  }

  func getLeft() -> Left? {
    switch self {
    case .left(let v): return v
    case .right: return nil
    }
  }

  func getRight() -> Right? {
    switch self {
    case .left: return nil
    case .right(let v): return v
    }
  }

  func mapLeft<NewLeft>(_ transform: (Left) -> NewLeft) -> Either<NewLeft, Right> {
    switch self {
    case .left(let v): return .left(transform(v))
    case .right(let v): return .right(v)
    }
  }

  func mapRight<NewRight>(_ transform: (Right) -> NewRight) -> Either<Left, NewRight> {
    switch self {
    case .left(let v): return .left(v)
    case .right(let v): return .right(transform(v))
    }
  }
}

// --- Generic class ---

class Stack<Element> {
  var items: [Element]

  init() {
    self.items = []
  }

  func push(_ item: Element) -> Void {
    // adds to items
  }

  func isEmpty() -> Bool {
    return items == []
  }
}

// --- Generic function taking closures ---

func transform<T, U>(_ value: T, using f: (T) -> U) -> U {
  return f(value)
}

func combine<A, B, C>(_ a: A, _ b: B, using f: (A, B) -> C) -> C {
  return f(a, b)
}

// --- Chained generics ---

func doubleMap<A, B, C>(_ value: A, _ f: (A) -> B, _ g: (B) -> C) -> C {
  return g(f(value))
}
