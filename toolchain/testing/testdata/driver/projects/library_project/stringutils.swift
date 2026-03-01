// String utility functions for library project testing.

func repeatString(_ s: String, _ count: Int) -> String {
  var result: String = ""
  var i: Int = 0
  while i < count {
    result = result + s
    i = i + 1
  }
  return result
}

func isPalindrome(_ s: String) -> Bool {
  let reversed: String = __tinyswift_string_reverse(s)
  return s == reversed
}
