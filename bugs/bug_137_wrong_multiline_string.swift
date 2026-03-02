// BUG 137: [WRONG] Multiline string literals not properly parsed
//
// Triple-quoted multiline string literals (""") are not parsed correctly.
// The output includes the `""` markers and does not strip leading
// indentation as Swift requires.
//
// EXPECTED: prints "hello\nworld"
// ACTUAL: prints "\"\""\n    hello\n    world\n    \"\""
//
// WORKAROUND: Use single-line strings with explicit \n or string
// concatenation.

func main() -> Int {
  let s: String = """
    hello
    world
    """
  print(s)
  return 0
}
