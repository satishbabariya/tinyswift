// BUG 60: [CRASH-CG] willSet/didSet property observers crash codegen
//
// Using willSet or didSet property observers on struct fields crashes the
// compiler with SIL error ("non-void function missing return") followed
// by LLVM StoreInst assertion.
//
// EXPECTED: compiles and prints "will change\n10"
// ACTUAL: compiler crash (SIL + LLVM assertions)
//
// Build: tinyswift compile --no-prelude-import bugs/bug_60_crash_cg_property_observers.swift

struct Watched {
  var value: Int {
    willSet {               // COMPILER CRASH
      print("will change")
    }
  }
}

func main() -> Int {
  var w: Watched = Watched(value: 0)
  w.value = 10
  print(w.value)
  return 0
}
