// TinySwift Example: Enums
// Demonstrates enum declarations, associated values, methods, and generics.

// --- Basic enum ---

enum Season {
  case spring
  case summer
  case autumn
  case winter
}

func seasonName(_ s: Season) -> String {
  switch s {
  case .spring: return "Spring"
  case .summer: return "Summer"
  case .autumn: return "Autumn"
  case .winter: return "Winter"
  }
}

// --- Enum with methods ---

enum Coin {
  case penny
  case nickel
  case dime
  case quarter

  func value() -> Int {
    switch self {
    case .penny: return 1
    case .nickel: return 5
    case .dime: return 10
    case .quarter: return 25
    }
  }

  func name() -> String {
    switch self {
    case .penny: return "penny"
    case .nickel: return "nickel"
    case .dime: return "dime"
    case .quarter: return "quarter"
    }
  }
}

// --- Enum with associated values ---

enum Measurement {
  case inches(Double)
  case centimeters(Double)
  case meters(Double)
}

func toMeters(_ m: Measurement) -> Double {
  switch m {
  case .inches(let val): return val * 0.0254
  case .centimeters(let val): return val / 100.0
  case .meters(let val): return val
  }
}

// --- Enum with multiple associated values ---

enum NetworkResponse {
  case success(Int, String)
  case error(Int, String)
  case timeout
}

func handleResponse(_ resp: NetworkResponse) -> String {
  switch resp {
  case .success(let code, let body):
    return body
  case .error(let code, let msg):
    return msg
  case .timeout:
    return "Request timed out"
  }
}

// --- Generic enum (demonstrates the pattern; prelude provides Optional<T>) ---

enum Maybe<Wrapped> {
  case nothing
  case just(Wrapped)
}

// --- Generic enum with methods (demonstrates Result pattern; prelude provides Result<S,F>) ---

enum Outcome<Success, Failure> {
  case success(Success)
  case failure(Failure)

  func isSuccess() -> Bool {
    switch self {
    case .success: return true
    case .failure: return false
    }
  }

  func isFailure() -> Bool {
    switch self {
    case .success: return false
    case .failure: return true
    }
  }

  func getSuccess() -> Success? {
    switch self {
    case .success(let v): return v
    case .failure: return nil
    }
  }

  func getFailure() -> Failure? {
    switch self {
    case .success: return nil
    case .failure(let e): return e
    }
  }

  func map<NewSuccess>(_ transform: (Success) -> NewSuccess) -> Outcome<NewSuccess, Failure> {
    switch self {
    case .success(let v): return Outcome<NewSuccess, Failure>.success(transform(v))
    case .failure(let e): return Outcome<NewSuccess, Failure>.failure(e)
    }
  }

  func flatMap<NewSuccess>(_ transform: (Success) -> Outcome<NewSuccess, Failure>) -> Outcome<NewSuccess, Failure> {
    switch self {
    case .success(let v): return transform(v)
    case .failure(let e): return Outcome<NewSuccess, Failure>.failure(e)
    }
  }
}

// --- Enum for state machines ---

enum ConnectionState {
  case disconnected
  case connecting
  case connected(Int)
  case failed(String)

  func isActive() -> Bool {
    switch self {
    case .connected: return true
    default: return false
    }
  }
}

// --- Enum with protocol conformance ---

enum Color: Equatable {
  case red
  case green
  case blue

  func equals(_ other: Color) -> Bool {
    switch self {
    case .red:
      switch other {
      case .red: return true
      default: return false
      }
    case .green:
      switch other {
      case .green: return true
      default: return false
      }
    case .blue:
      switch other {
      case .blue: return true
      default: return false
      }
    }
  }
}
