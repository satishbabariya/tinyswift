// BUG 155: [CRASH] Dictionary field in struct crashes codegen
//
// A struct with a [String: Int] field crashes with InsertValueInst
// assertion. Dictionaries as local variables work fine.
//
// EXPECTED: prints 42
// ACTUAL: Assertion "Inserted value must match indexed type!"
//
// WORKAROUND: Keep dictionaries as local variables, not struct fields.

struct DB {
  var data: [String: Int]    // CRASH
}

func main() -> Int {
  var db: DB = DB(data: [:])
  db.data["count"] = 42
  print(db.data["count"] ?? 0)
  return 0
}
