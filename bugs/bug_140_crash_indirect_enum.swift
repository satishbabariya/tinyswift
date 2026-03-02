// BUG 140: [CRASH] `indirect enum` crashes codegen
//
// Enum types declared with the `indirect` keyword crash during
// compilation with a stack dump.
//
// EXPECTED: prints 6
// ACTUAL: compiler crash (stack dump)
//
// WORKAROUND: Use arrays of enum values to simulate recursive structures.

indirect enum Tree {
  case leaf(Int)
  case node(Tree, Tree)
}

func sum(_ t: Tree) -> Int {
  switch t {
  case .leaf(let v): return v
  case .node(let l, let r): return sum(l) + sum(r)
  }
}

func main() -> Int {
  let tree: Tree = Tree.node(Tree.leaf(1), Tree.node(Tree.leaf(2), Tree.leaf(3)))
  print(sum(tree))
  return 0
}
