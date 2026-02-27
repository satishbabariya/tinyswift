// TinySwift Example: Conditional Compilation
// Demonstrates #if, #else, #elseif, #endif, and preprocessor directives.

// --- Basic conditional compilation ---

#if DEBUG
func logMessage(_ msg: String) -> Void {
  // Only compiled in debug builds
}
#endif

// --- If-else conditional compilation ---

#if PLATFORM_LINUX
let platformName: String = "Linux"
#else
let platformName: String = "Unknown"
#endif

// --- If-elseif-else chain ---

#if ARCH_X86_64
let archName: String = "x86_64"
#elseif ARCH_ARM64
let archName: String = "arm64"
#else
let archName: String = "unknown"
#endif

// --- Availability checking ---

#if #available(TinySwift 2.0)
func newFeature() -> Int {
  return 42
}
#endif

// --- Compile-time diagnostics ---

// #warning and #error directives for compile-time messages
// #warning("This feature is experimental")
// #error("This platform is not supported")

// --- Source location directives ---

// These expand to compile-time constants:
// #file      - current file path
// #filePath  - full file path
// #fileID    - module/file identifier
// #line      - current line number
// #column    - current column number
// #function  - current function name

func debugLocation() -> String {
  return #file
}

func currentLine() -> Int {
  return #line
}

// --- Compile-time assertions ---

// #assert is used for compile-time checks
// #assert(true, "This should always pass")

// --- Nested conditional compilation ---

#if DEBUG
  #if VERBOSE
  func verboseDebugLog(_ msg: String) -> Void {
    // Only in verbose debug mode
  }
  #else
  func debugLog(_ msg: String) -> Void {
    // Only in debug mode
  }
  #endif
#endif

// --- Conditional compilation around declarations ---

#if FEATURE_ASYNC
func asyncOperation() async -> Int {
  return 1
}
#endif

#if FEATURE_GENERATORS
func generateSequence() -> Generator<Int> {
  yield 1
  yield 2
  yield 3
}
#endif

// --- Conditional compilation with structs ---

#if USE_DOUBLE_PRECISION
struct Vector {
  var x: Double
  var y: Double
}
#else
struct Vector {
  var x: Int
  var y: Int
}
#endif
