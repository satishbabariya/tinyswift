// TinySwift Example: C Interop
// Demonstrates @extern("C") for importing C functions and @cdecl for exporting.

// --- Importing C functions with @extern("C") ---

// Math functions from libm
@extern("C") func __tinyswift_sqrt(_ x: Double) -> Double
@extern("C") func __tinyswift_pow(_ base: Double, _ exp: Double) -> Double
@extern("C") func __tinyswift_log(_ x: Double) -> Double
@extern("C") func __tinyswift_sin(_ x: Double) -> Double
@extern("C") func __tinyswift_cos(_ x: Double) -> Double
@extern("C") func __tinyswift_floor(_ x: Double) -> Double
@extern("C") func __tinyswift_ceil(_ x: Double) -> Double

// String functions
@extern("C") func __tinyswift_string_hash(_ s: String) -> Int
@extern("C") func __tinyswift_string_compare(_ a: String, _ b: String) -> Int

// Integer functions
@extern("C") func __tinyswift_int_abs(_ x: Int) -> Int
@extern("C") func __tinyswift_int_to_string(_ x: Int) -> String

// System functions
@extern("C") func __tinyswift_abort(_ message: String) -> Void
@extern("C") func __tinyswift_clock_now() -> Int
@extern("C") func __tinyswift_clock_monotonic() -> Int

// --- Thin wrappers around C functions ---

func sqrt(_ x: Double) -> Double { return __tinyswift_sqrt(x) }
func pow(_ base: Double, _ exp: Double) -> Double { return __tinyswift_pow(base, exp) }
func log(_ x: Double) -> Double { return __tinyswift_log(x) }
func sin(_ x: Double) -> Double { return __tinyswift_sin(x) }
func cos(_ x: Double) -> Double { return __tinyswift_cos(x) }
func floor(_ x: Double) -> Double { return __tinyswift_floor(x) }
func ceil(_ x: Double) -> Double { return __tinyswift_ceil(x) }

func clockNow() -> Int { return __tinyswift_clock_now() }
func clockMonotonic() -> Int { return __tinyswift_clock_monotonic() }

// --- Exporting TinySwift functions to C with @cdecl ---

@cdecl("get_answer")
func answer() -> Int {
  return 42
}

@cdecl("add_ints")
func addInts(_ a: Int, _ b: Int) -> Int {
  return a + b
}

@cdecl("multiply_doubles")
func multiplyDoubles(_ a: Double, _ b: Double) -> Double {
  return a * b
}

@cdecl("is_positive")
func isPositive(_ n: Int) -> Bool {
  return n > 0
}

@cdecl("compute_distance")
func computeDistance(_ x1: Double, _ y1: Double, _ x2: Double, _ y2: Double) -> Double {
  let dx: Double = x2 - x1
  let dy: Double = y2 - y1
  return __tinyswift_sqrt(dx * dx + dy * dy)
}

// --- Using imported C functions in higher-level code ---

func fatalError(_ message: String) -> Void {
  __tinyswift_abort(message)
}

func precondition(_ condition: Bool, _ message: String) -> Void {
  if !condition {
    __tinyswift_abort(message)
  }
}

func hypotenuse(_ a: Double, _ b: Double) -> Double {
  return __tinyswift_sqrt(a * a + b * b)
}

func circleArea(_ radius: Double) -> Double {
  let pi: Double = 3.14159265358979323846
  return pi * radius * radius
}
