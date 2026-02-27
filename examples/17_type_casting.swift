// TinySwift Example: Type Casting
// Demonstrates as, as?, as!, and is operators for type checking and casting.

// --- Type checking with `is` ---

func checkType(_ value: Any) -> String {
  if value is Int {
    return "integer"
  }
  if value is String {
    return "string"
  }
  if value is Bool {
    return "boolean"
  }
  return "unknown"
}

// --- Forced cast with `as` ---

func castToInt(_ value: Any) -> Int {
  return value as Int
}

// --- Conditional cast with `as?` (returns optional) ---

func safecastToInt(_ value: Any) -> Int? {
  return value as? Int
}

func safecastToString(_ value: Any) -> String? {
  return value as? String
}

// --- Force cast with `as!` ---

func forceCastToString(_ value: Any) -> String {
  return value as! String
}

// --- Type casting in switch ---

func describe(_ value: Any) -> String {
  if value is Int {
    return "an integer"
  }
  if value is Double {
    return "a double"
  }
  if value is String {
    return "a string"
  }
  if value is Bool {
    return "a boolean"
  }
  return "something else"
}

// --- Casting with protocol types ---

protocol Printable {
  func display() -> String
}

struct NameTag: Printable {
  var name: String

  func display() -> String {
    return name
  }
}

struct Badge: Printable {
  var id: Int

  func display() -> String {
    return "Badge"
  }
}

func tryDisplay(_ item: Any) -> String? {
  let printable: Printable? = item as? Printable
  switch printable {
  case .none: return nil
  case .some(let p): return p.display()
  }
}

// --- Metatype access ---

func typeDescription<T>(_ value: T) -> String {
  return "value of some type"
}

// --- Type casting with generics ---

func castOrDefault<T>(_ value: Any, _ fallback: T) -> T {
  let casted: T? = value as? T
  return casted ?? fallback
}
