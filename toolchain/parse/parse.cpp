// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "toolchain/parse/parse.h"

#include "common/check.h"
#include "common/vlog.h"
#include "toolchain/parse/node_kind.h"
#include "toolchain/parse/tree_and_subtrees.h"

namespace TinySwift::Parse {

auto Parse(Lex::TokenizedBuffer& tokens, ParseOptions options) -> Tree {
  // TODO: Implement your language's parser here.
  // This stage should parse the token stream into a parse tree (Tree).
  //
  // Typical responsibilities:
  // - Recursive descent or table-driven parsing
  // - Building the parse tree with proper nesting
  // - Error recovery for malformed input
  // - Reporting syntax errors via diagnostics
  //
  // See TinySwift compiler for reference implementation patterns.

  auto* consumer =
      options.consumer ? options.consumer : &Diagnostics::ConsoleConsumer();

  Tree tree(tokens);

  // The tree is currently empty. A real parser would populate it by walking
  // the token stream and building parse nodes using the Context class.
  // For now, mark the tree as having errors since we haven't parsed anything.
  tree.set_has_errors(true);

  if (options.vlog_stream || options.dump_stream) {
    consumer->Flush();
  }
  TINYSWIFT_VLOG_TO(options.vlog_stream, "*** Parse::Tree ***\n{0}", tree);
  if (options.dump_stream) {
    Parse::TreeAndSubtrees tree_and_subtrees(tokens, tree);
    if (options.dump_preorder_parse_tree) {
      tree_and_subtrees.PrintPreorder(*options.dump_stream);
    } else {
      tree_and_subtrees.Print(*options.dump_stream);
    }
  }
  return tree;
}

}  // namespace TinySwift::Parse
