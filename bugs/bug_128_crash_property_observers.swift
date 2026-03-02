// BUG 128: [CRASH] Property observers (willSet/didSet) crash codegen
//
// Properties with willSet or didSet observers cause "non-void function
// missing return" errors and StoreInst assertions.
//
// EXPECTED: prints "changing" then 10
// ACTUAL: error: non-void function missing return / StoreInst assertion
//
// WORKAROUND: Use a setter method instead of property observers.

struct Tracked {
  var value: Int {
    willSet { print("changing") }   // CRASH
  }
}

func main() -> Int {
  var t: Tracked = Tracked(value: 0)
  t.value = 10
  print(t.value)
  return 0
}
