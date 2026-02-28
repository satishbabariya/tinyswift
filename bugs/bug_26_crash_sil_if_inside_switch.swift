// BUG 26: [CRASH-SIL] if inside switch case arm
//
// Using if/else inside a switch case body causes SIL error.
//
// EXPECTED: compiles and runs
// ACTUAL: SIL error: "cond_br missing condition operand"
//
// WORKAROUND: Extract the if-logic into a separate function.
//
// Build: tinyswift compile --no-prelude-import bugs/bug_26_crash_sil_if_inside_switch.swift

enum Action {
  case deposit(Int)
  case withdraw(Int)
}

func process(_ action: Action, _ balance: Int) -> Int {
  switch action {
  case .deposit(let amount):
    return balance + amount
  case .withdraw(let amount):
    if amount <= balance {
      return balance - amount
    }
    return balance  // insufficient funds
  }
}

func main() -> Int {
  print(process(Action.deposit(100), 50))
  return 0
}
