// BUG 79: [CRASH-CG] Dictionary with non-String key type crashes
//
// Dictionary literals with String keys work, but using Int (or other)
// key types crashes with LLVM function signature assertion.
//
// EXPECTED: compiles and prints "one"
// ACTUAL: compiler crash (LLVM bad signature assertion)
//
// Build: tinyswift compile --no-prelude-import bugs/bug_79_crash_cg_dict_int_key.swift

func main() -> Int {
  let d: [Int: String] = [1: "one", 2: "two"]   // COMPILER CRASH
  print(d[1]!)
  return 0
}
