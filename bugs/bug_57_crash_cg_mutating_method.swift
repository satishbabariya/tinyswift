// BUG 57: [CRASH-CG] Mutating struct methods crash codegen
//
// Declaring a `mutating func` on a struct crashes the compiler with
// SIL verification error "return missing operand" followed by LLVM
// StoreInst assertion. Affects all mutating methods regardless of
// whether they take parameters.
//
// Non-mutating methods and regular functions work fine.
//
// EXPECTED: compiles and prints 3
// ACTUAL: compiler crash (SIL + LLVM assertions)
//
// Build: tinyswift compile --no-prelude-import bugs/bug_57_crash_cg_mutating_method.swift

struct Counter {
  var count: Int
  mutating func increment() {   // COMPILER CRASH
    count = count + 1
  }
}

func main() -> Int {
  var c: Counter = Counter(count: 0)
  c.increment()
  c.increment()
  c.increment()
  print(c.count)
  return 0
}
