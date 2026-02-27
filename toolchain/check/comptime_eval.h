// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef TINYSWIFT_TOOLCHAIN_CHECK_COMPTIME_EVAL_H_
#define TINYSWIFT_TOOLCHAIN_CHECK_COMPTIME_EVAL_H_

#include <optional>
#include <string>
#include <vector>

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringRef.h"
#include "toolchain/parse/tree.h"
#include "toolchain/sem_ir/ids.h"

namespace TinySwift::Check {

class Context;

// A compile-time value produced by the comptime interpreter.
struct ComptimeValue {
  enum Kind : uint8_t { None, Int, Bool, Float, String, Array, Struct };
  Kind kind = None;
  int64_t int_val = 0;
  bool bool_val = false;
  double float_val = 0.0;
  std::string string_val;
  std::vector<ComptimeValue> array_vals;
  std::vector<std::pair<std::string, ComptimeValue>> struct_fields;
  std::string struct_type_name;

  static auto MakeNone() -> ComptimeValue { return ComptimeValue(); }
  static auto MakeInt(int64_t v) -> ComptimeValue {
    ComptimeValue val;
    val.kind = Int;
    val.int_val = v;
    return val;
  }
  static auto MakeBool(bool v) -> ComptimeValue {
    ComptimeValue val;
    val.kind = Bool;
    val.bool_val = v;
    return val;
  }
  static auto MakeFloat(double v) -> ComptimeValue {
    ComptimeValue val;
    val.kind = Float;
    val.float_val = v;
    return val;
  }
  static auto MakeString(llvm::StringRef v) -> ComptimeValue {
    ComptimeValue val;
    val.kind = String;
    val.string_val = v.str();
    return val;
  }
  static auto MakeArray(std::vector<ComptimeValue> elems) -> ComptimeValue {
    ComptimeValue val;
    val.kind = Array;
    val.array_vals = std::move(elems);
    return val;
  }
  static auto MakeStruct(llvm::StringRef type_name,
                          std::vector<std::pair<std::string, ComptimeValue>> fields)
      -> ComptimeValue {
    ComptimeValue val;
    val.kind = Struct;
    val.struct_type_name = type_name.str();
    val.struct_fields = std::move(fields);
    return val;
  }

  auto IsNumeric() const -> bool { return kind == Int || kind == Float; }
  auto IsNone() const -> bool { return kind == None; }
  auto ToInt64() const -> int64_t {
    if (kind == Int) return int_val;
    if (kind == Float) return static_cast<int64_t>(float_val);
    if (kind == Bool) return bool_val ? 1 : 0;
    return 0;
  }
  auto ToDouble() const -> double {
    if (kind == Float) return float_val;
    if (kind == Int) return static_cast<double>(int_val);
    return 0.0;
  }
  auto ToBool() const -> bool {
    if (kind == Bool) return bool_val;
    if (kind == Int) return int_val != 0;
    return false;
  }
};

// Tree-walking interpreter for compile-time evaluation.
// Operates entirely in the check phase — results replace comptime expressions
// with literal SemIR constants (IntValue, BoolLiteral, etc.).
class ComptimeEvaluator {
 public:
  explicit ComptimeEvaluator(Context& context);

  // Entry point: evaluate `comptime <expr>` parse node -> SemIR literal InstId.
  auto EvaluateComptimeExpr(Parse::NodeId node_id) -> SemIR::InstId;

  // M113: Register a comptime function definition.
  auto RegisterComptimeFunction(SemIR::FunctionId fn_id,
                                Parse::NodeId def_node_id) -> void;

  // M113: Query whether a function is comptime.
  auto IsComptimeFunction(SemIR::FunctionId fn_id) const -> bool;

 private:
  // Evaluate a parse-tree expression node to a ComptimeValue.
  auto EvalExpr(Parse::NodeId node_id) -> std::optional<ComptimeValue>;

  // Execute a statement node. Returns false if a return was hit.
  auto EvalStmt(Parse::NodeId node_id) -> bool;

  // M113: Execute a comptime function body with the given arguments.
  auto ExecuteFunction(SemIR::FunctionId fn_id,
                       llvm::ArrayRef<ComptimeValue> args)
      -> std::optional<ComptimeValue>;

  // Convert a ComptimeValue back to a SemIR literal instruction.
  auto ValueToSemIR(Parse::NodeId loc, const ComptimeValue& val) -> SemIR::InstId;

  // M113: Evaluate a function call expression.
  auto EvalCallExpr(Parse::NodeId node_id) -> std::optional<ComptimeValue>;

  // M114: Evaluate a struct initialization call.
  auto EvalStructInit(Parse::NodeId node_id, llvm::StringRef type_name,
                      llvm::ArrayRef<ComptimeValue> args)
      -> std::optional<ComptimeValue>;

  // M114: Evaluate a member access expression.
  auto EvalMemberAccessExpr(Parse::NodeId node_id) -> std::optional<ComptimeValue>;

  // M114: Evaluate a subscript access expression.
  auto EvalSubscriptExpr(Parse::NodeId node_id) -> std::optional<ComptimeValue>;

  // Assignment helper for mutable variable updates.
  auto EvalAssignment(Parse::NodeId node_id) -> bool;

  // Execute all statements in a CodeBlock, handling condition-as-sibling
  // pattern (condition expressions appear as siblings before IfStatement
  // and WhileStatement, not as children of them).
  auto EvalCodeBlockBody(Parse::NodeId code_block_node) -> bool;

  // Scoped variable environment.
  void PushScope();
  void PopScope();
  void SetVar(llvm::StringRef name, ComptimeValue val);
  auto GetVar(llvm::StringRef name) -> std::optional<ComptimeValue>;
  void SetVarMutable(llvm::StringRef name, ComptimeValue val);

  // Safety check for iteration limits.
  auto CheckIterationLimit(Parse::NodeId loc) -> bool;

  Context& context_;
  std::vector<llvm::StringMap<ComptimeValue>> env_stack_;
  std::optional<ComptimeValue> return_value_;
  bool has_returned_ = false;
  llvm::DenseMap<int32_t, Parse::NodeId> comptime_function_defs_;
  // Condition-as-sibling: condition expressions appear before IfStatement/
  // WhileStatement as siblings, not children. This field holds the most
  // recently evaluated condition for the next If/While to consume.
  std::optional<Parse::NodeId> pending_condition_;
  static constexpr int kMaxIterations = 1000000;
  int iteration_count_ = 0;
};

}  // namespace TinySwift::Check

#endif  // TINYSWIFT_TOOLCHAIN_CHECK_COMPTIME_EVAL_H_
