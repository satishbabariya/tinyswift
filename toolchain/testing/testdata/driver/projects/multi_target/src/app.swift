// Main application that uses the mathlib.

func main() -> Int {
  let sum: Int = add(3, 4)
  let product: Int = multiply(5, 6)
  let fact: Int = factorial(5)
  print("sum = " + __tinyswift_int_to_string(sum))
  print("product = " + __tinyswift_int_to_string(product))
  print("factorial = " + __tinyswift_int_to_string(fact))
  return 0
}
