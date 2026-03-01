// Tests for utility functions.

func testIsEven() {
  assert(isEven(2))
  assert(isEven(0))
  assert(!isEven(1))
  assert(!isEven(3))
  assert(isEven(100))
}

func testMax() {
  assert(max(1, 2) == 2)
  assert(max(5, 3) == 5)
  assert(max(0, 0) == 0)
  assert(max(-1, 1) == 1)
}

func testClamp() {
  assert(clamp(5, 0, 10) == 5)
  assert(clamp(-1, 0, 10) == 0)
  assert(clamp(15, 0, 10) == 10)
  assert(clamp(0, 0, 10) == 0)
  assert(clamp(10, 0, 10) == 10)
}
