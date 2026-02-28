// TinySwift Example: C Interop
// Demonstrates @extern("C") for importing C functions and @cdecl for exporting.
// Note: The prelude already imports the standard __tinyswift_* C functions and
// provides wrappers (sqrt, pow, sin, cos, fatalError, precondition, etc.).
// This example shows how to import additional C functions and export
// TinySwift functions to C.

// --- Importing additional C functions with @extern("C") ---
// These demonstrate the syntax for C function declarations.

@extern("C") func __tinyswift_clock_now() -> Int
@extern("C") func __tinyswift_clock_monotonic() -> Int

// --- Wrappers for imported functions ---

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
  return sqrt(dx * dx + dy * dy)
}

// --- Using prelude-provided C wrappers in higher-level code ---

func hypotenuse(_ a: Double, _ b: Double) -> Double {
  return sqrt(a * a + b * b)
}

func circleArea(_ radius: Double) -> Double {
  return pi * radius * radius
}

func degToRad(_ degrees: Double) -> Double {
  return degrees * pi / 180.0
}

func radToDeg(_ radians: Double) -> Double {
  return radians * 180.0 / pi
}
