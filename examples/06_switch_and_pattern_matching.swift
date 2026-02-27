// TinySwift Example: Switch Statements and Pattern Matching
// Demonstrates switch/case/default, enum patterns, and value binding.

// --- Basic switch on Int ---

func dayName(_ day: Int) -> String {
  switch day {
  case 1: return "Monday"
  case 2: return "Tuesday"
  case 3: return "Wednesday"
  case 4: return "Thursday"
  case 5: return "Friday"
  case 6: return "Saturday"
  case 7: return "Sunday"
  default: return "Unknown"
  }
}

// --- Switch with enum ---

enum Direction {
  case north
  case south
  case east
  case west
}

func opposite(_ d: Direction) -> Direction {
  switch d {
  case .north: return .south
  case .south: return .north
  case .east: return .west
  case .west: return .east
  }
}

func directionName(_ d: Direction) -> String {
  switch d {
  case .north: return "North"
  case .south: return "South"
  case .east: return "East"
  case .west: return "West"
  }
}

// --- Switch with associated values and value binding ---

enum Shape {
  case circle(Double)
  case rectangle(Double, Double)
  case triangle(Double, Double)
}

func area(_ shape: Shape) -> Double {
  switch shape {
  case .circle(let radius):
    return 3.14159 * radius * radius
  case .rectangle(let width, let height):
    return width * height
  case .triangle(let base, let height):
    return 0.5 * base * height
  }
}

func describe(_ shape: Shape) -> String {
  switch shape {
  case .circle: return "circle"
  case .rectangle: return "rectangle"
  case .triangle: return "triangle"
  }
}

// --- Switch with multiple statements per case ---

enum TrafficLight {
  case red
  case yellow
  case green
}

func action(_ light: TrafficLight) -> String {
  switch light {
  case .red:
    return "Stop"
  case .yellow:
    return "Caution"
  case .green:
    return "Go"
  }
}

// --- Switch with fallthrough ---

func gradeDescription(_ grade: Int) -> String {
  switch grade {
  case 5: return "Excellent"
  case 4: return "Good"
  case 3: return "Satisfactory"
  case 2: return "Poor"
  case 1: return "Failing"
  default: return "Invalid"
  }
}

// --- Wildcard pattern in switch ---

func isWeekend(_ day: Int) -> Bool {
  switch day {
  case 6: return true
  case 7: return true
  default: return false
  }
}

// --- Pattern matching with optionals ---

func describeOptional(_ value: Int?) -> String {
  switch value {
  case .none: return "empty"
  case .some(let v): return "has value"
  }
}
