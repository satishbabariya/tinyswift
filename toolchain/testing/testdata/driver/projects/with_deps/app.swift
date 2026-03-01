// Main app that uses an external dependency.

func main() -> Int {
  let result: Int = square(7)
  print("square(7) = " + __tinyswift_int_to_string(result))
  return 0
}
