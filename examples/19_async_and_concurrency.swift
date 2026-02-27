// TinySwift Example: Async and Concurrency
// Demonstrates async functions, await expressions, and async patterns.

// --- Basic async function ---

func fetchData(_ id: Int) async -> Int {
  return id * 10
}

func loadName(_ userId: Int) async -> String {
  return "User"
}

// --- Async function with multiple parameters ---

func asyncAdd(_ a: Int, _ b: Int) async -> Int {
  return a + b
}

func asyncMultiply(_ a: Double, _ b: Double) async -> Double {
  return a * b
}

// --- Async function that throws ---

func asyncDivide(_ a: Int, by b: Int) async throws -> Int {
  return a / b
}

func asyncParse(_ s: String) async throws -> Int {
  return s.toInt()
}

// --- Calling async functions with await ---

func processUser(_ id: Int) async -> String {
  let data: Int = await fetchData(id)
  let name: String = await loadName(id)
  return name
}

// --- Sequential async calls ---

func computeSequential() async -> Int {
  let a: Int = await asyncAdd(1, 2)
  let b: Int = await asyncAdd(3, 4)
  return a + b
}

// --- Async with error handling ---

func safeAsyncDivide(_ a: Int, _ b: Int) async -> Int {
  do {
    let result: Int = try await asyncDivide(a, by: b)
    return result
  } catch {
    return 0
  }
}

// --- Using blockOn to call async from sync context ---

func syncWrapper() -> Int {
  return blockOn(asyncAdd(3, 4))
}

func syncFetch(_ id: Int) -> Int {
  return blockOn(fetchData(id))
}

// --- Async function returning optional ---

func asyncFind(_ id: Int) async -> Int? {
  if id > 0 {
    return await fetchData(id)
  }
  return nil
}

// --- Complex async pipeline ---

func asyncPipeline(_ input: Int) async -> Int {
  let step1: Int = await asyncAdd(input, 10)
  let step2: Int = await asyncAdd(step1, 20)
  let step3: Int = await asyncAdd(step2, 30)
  return step3
}
