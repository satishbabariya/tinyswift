// TinySwift Algorithmic Example: State Machine
// Demonstrates: enums with associated values, switch, structs, methods, state transitions.
// Build: tinyswift compile examples/state_machine.swift
// Run:   ./state_machine

// --- Traffic light state machine ---

enum TrafficLight {
  case red
  case green
  case yellow
}

func nextLight(_ current: TrafficLight) -> TrafficLight {
  switch current {
  case .red: return TrafficLight.green
  case .green: return TrafficLight.yellow
  case .yellow: return TrafficLight.red
  }
}

func lightName(_ light: TrafficLight) -> String {
  switch light {
  case .red: return "RED"
  case .green: return "GREEN"
  case .yellow: return "YELLOW"
  }
}

func lightDuration(_ light: TrafficLight) -> Int {
  switch light {
  case .red: return 30
  case .green: return 25
  case .yellow: return 5
  }
}

// --- Vending machine state ---

enum VendingState {
  case idle
  case hasCoins(Int)
  case dispensing(String)
  case error(String)
}

func vendingStateName(_ state: VendingState) -> String {
  switch state {
  case .idle: return "IDLE"
  case .hasCoins: return "HAS_COINS"
  case .dispensing: return "DISPENSING"
  case .error: return "ERROR"
  }
}

func insertCoin(_ state: VendingState, _ amount: Int) -> VendingState {
  switch state {
  case .idle:
    return VendingState.hasCoins(amount)
  case .hasCoins(let current):
    return VendingState.hasCoins(current + amount)
  case .dispensing:
    return VendingState.error("busy dispensing")
  case .error:
    return state
  }
}

func selectItem(_ state: VendingState, _ price: Int, _ name: String) -> VendingState {
  switch state {
  case .idle:
    return VendingState.error("insert coins first")
  case .hasCoins(let amount):
    if amount >= price {
      return VendingState.dispensing(name)
    }
    return VendingState.error("insufficient funds")
  case .dispensing:
    return VendingState.error("already dispensing")
  case .error:
    return state
  }
}

func collectItem(_ state: VendingState) -> VendingState {
  switch state {
  case .dispensing:
    return VendingState.idle
  default:
    return state
  }
}

// --- Connection state machine ---

enum ConnState {
  case disconnected
  case connecting
  case connected(Int)
  case error(String)
}

func connect(_ state: ConnState) -> ConnState {
  switch state {
  case .disconnected:
    return ConnState.connecting
  case .connecting:
    return state // already connecting
  case .connected:
    return state // already connected
  case .error:
    return ConnState.connecting // retry
  }
}

func onConnected(_ state: ConnState, _ sessionId: Int) -> ConnState {
  switch state {
  case .connecting:
    return ConnState.connected(sessionId)
  default:
    return state
  }
}

func disconnect(_ state: ConnState) -> ConnState {
  switch state {
  case .connected:
    return ConnState.disconnected
  case .connecting:
    return ConnState.disconnected
  default:
    return state
  }
}

func onError(_ state: ConnState, _ msg: String) -> ConnState {
  return ConnState.error(msg)
}

func isConnected(_ state: ConnState) -> Bool {
  switch state {
  case .connected: return true
  default: return false
  }
}

// --- Simple counter with bounds ---

struct BoundedCounter {
  var value: Int
  var minimum: Int
  var maximum: Int

  func increment() -> BoundedCounter {
    if value < maximum {
      return BoundedCounter(value: value + 1, minimum: minimum, maximum: maximum)
    }
    return self
  }

  func decrement() -> BoundedCounter {
    if value > minimum {
      return BoundedCounter(value: value - 1, minimum: minimum, maximum: maximum)
    }
    return self
  }

  func isAtMin() -> Bool {
    return value == minimum
  }

  func isAtMax() -> Bool {
    return value == maximum
  }
}

// --- Entry point ---

func main() -> Int {
  // Traffic light simulation
  print("Traffic light cycle:")
  var light: TrafficLight = TrafficLight.red
  var cycle: Int = 0
  while cycle < 6 {
    print(lightName(light))
    print(lightDuration(light))
    light = nextLight(light)
    cycle = cycle + 1
  }

  // Vending machine
  print("Vending machine:")
  var vend: VendingState = VendingState.idle
  print(vendingStateName(vend))  // IDLE

  vend = insertCoin(vend, 50)
  print(vendingStateName(vend))  // HAS_COINS

  vend = insertCoin(vend, 25)
  print(vendingStateName(vend))  // HAS_COINS

  // Try to buy (price=100, but we only have 75)
  vend = selectItem(vend, 100, "Soda")
  print(vendingStateName(vend))  // ERROR

  // Reset and try again
  vend = VendingState.idle
  vend = insertCoin(vend, 100)
  vend = selectItem(vend, 100, "Soda")
  print(vendingStateName(vend))  // DISPENSING

  vend = collectItem(vend)
  print(vendingStateName(vend))  // IDLE

  // Connection state machine
  print("Connection:")
  var conn: ConnState = ConnState.disconnected

  conn = connect(conn)
  if isConnected(conn) {
    print("connected")
  } else {
    print("not connected yet")
  }

  conn = onConnected(conn, 42)
  if isConnected(conn) {
    print("connected")
  } else {
    print("not connected")
  }

  conn = disconnect(conn)
  if isConnected(conn) {
    print("still connected")
  } else {
    print("disconnected")
  }

  // Error and retry
  conn = onError(conn, "timeout")
  conn = connect(conn) // retry from error
  conn = onConnected(conn, 99)
  if isConnected(conn) {
    print("reconnected")
  }

  // Bounded counter
  print("Bounded counter (0..5):")
  var counter: BoundedCounter = BoundedCounter(value: 0, minimum: 0, maximum: 5)
  var step: Int = 0
  while step < 7 {
    print(counter.value)
    counter = counter.increment()
    step = step + 1
  }
  // Should print 0 1 2 3 4 5 5 (capped at 5)

  return 0
}
