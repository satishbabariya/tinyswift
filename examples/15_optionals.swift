// TinySwift Example: Optionals
// Demonstrates optional types, nil, unwrapping, and nil coalescing.

// --- Optional declaration ---

let noValue: Int? = nil
let hasValue: Int? = 42
let noString: String? = nil
let hasString: String? = "hello"

// --- Force unwrapping with ! ---

func forceGet(_ opt: Int?) -> Int {
  return opt!
}

// --- Nil coalescing with ?? ---

func getOrDefault(_ opt: Int?, _ fallback: Int) -> Int {
  return opt ?? fallback
}

func getStringOrDefault(_ opt: String?, _ fallback: String) -> String {
  return opt ?? fallback
}

// --- Optional in switch (pattern matching) ---

func describeOptional(_ value: Int?) -> String {
  switch value {
  case .none:
    return "no value"
  case .some(let v):
    return "has value"
  }
}

// --- Functions returning optionals ---

func safeDivide(_ a: Int, by b: Int) -> Int? {
  if b == 0 {
    return nil
  }
  return a / b
}

func findIndex(_ items: [Int], _ target: Int) -> Int? {
  var i: Int = 0
  for item in items {
    if item == target {
      return i
    }
    i = i + 1
  }
  return nil
}

// --- Chaining optional results ---

func doubleIfPresent(_ opt: Int?) -> Int? {
  switch opt {
  case .none: return nil
  case .some(let v): return v * 2
  }
}

func addIfBothPresent(_ a: Int?, _ b: Int?) -> Int? {
  switch a {
  case .none: return nil
  case .some(let va):
    switch b {
    case .none: return nil
    case .some(let vb): return va + vb
    }
  }
}

// --- Optional in struct fields ---

struct User {
  var name: String
  var email: String?
  var age: Int?

  func hasEmail() -> Bool {
    switch email {
    case .none: return false
    case .some: return true
    }
  }

  func hasAge() -> Bool {
    switch age {
    case .none: return false
    case .some: return true
    }
  }
}

// --- Implicitly unwrapped optionals ---

let implicitInt: Int! = 100

func useImplicit(_ value: Int!) -> Int {
  return value + 1
}

// --- Optional with enum associated values ---

enum SearchResult {
  case found(Int)
  case notFound
}

func toOptional(_ result: SearchResult) -> Int? {
  switch result {
  case .found(let idx): return idx
  case .notFound: return nil
  }
}

// --- Nested optionals ---

func flattenOptional(_ opt: Int??) -> Int? {
  switch opt {
  case .none: return nil
  case .some(let inner): return inner
  }
}
