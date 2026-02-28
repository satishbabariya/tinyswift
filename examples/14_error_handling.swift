// TinySwift Example: Error Handling
// Demonstrates throws, try, do-catch, and error propagation.

// --- Function that throws ---

func divide(_ a: Int, by b: Int) throws -> Int {
  return a / b
}

func parseNumber(_ s: String) throws -> Int {
  return s.toInt()
}

// --- Do-catch block ---

func safeDivide(_ a: Int, _ b: Int) -> Int {
  do {
    let result: Int = try divide(a, by: b)
    return result
  } catch {
    return 0
  }
}

// --- Try with optional result (try?) ---

func tryParse(_ s: String) -> Int? {
  return try? parseNumber(s)
}

// --- Try with force unwrap (try!) ---

func forceParse(_ s: String) -> Int {
  return try! parseNumber(s)
}

// --- Propagating errors with throws ---

func processAndDouble(_ s: String) throws -> Int {
  let value: Int = try parseNumber(s)
  return value * 2
}

func processChain(_ s: String) throws -> Int {
  let parsed: Int = try parseNumber(s)
  let result: Int = try divide(parsed, by: 2)
  return result
}

// --- Rethrows ---

func withRetry(_ body: () throws -> Int) rethrows -> Int {
  return try body()
}

// --- Do-catch with multiple try calls ---

func complexOperation(_ a: String, _ b: String) -> Int {
  do {
    let x: Int = try parseNumber(a)
    let y: Int = try parseNumber(b)
    let result: Int = try divide(x, by: y)
    return result
  } catch {
    return -1
  }
}

// --- Throw statement ---

func validateAge(_ age: Int) throws -> Bool {
  if age < 0 {
    throw "Invalid age"
  }
  if age > 150 {
    throw "Unrealistic age"
  }
  return true
}

// --- Error handling with Result enum (uses prelude's Result<S,F>) ---

func safeParseToResult(_ s: String) -> Result<Int, String> {
  do {
    let value: Int = try parseNumber(s)
    return .success(value)
  } catch {
    return .failure("Parse error")
  }
}
