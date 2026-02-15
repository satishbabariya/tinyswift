// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef TINYSWIFT_TOOLCHAIN_SEM_IR_TYPED_INSTS_H_
#define TINYSWIFT_TOOLCHAIN_SEM_IR_TYPED_INSTS_H_

#include "common/template_string.h"
#include "toolchain/base/int.h"
#include "toolchain/parse/node_ids.h"
#include "toolchain/parse/typed_nodes.h"
#include "toolchain/sem_ir/ids.h"
#include "toolchain/sem_ir/inst_kind.h"
#include "toolchain/sem_ir/singleton_insts.h"

// Representations for specific kinds of instructions.
//
// Each type should be a struct with the following members, in this order:
//
// - Either a `Kind` constant, or a `Kinds` constant and an `InstKind kind;`
//   member.
// - Optionally, a `TypeId type_id;` member, for instructions that produce a
//   value.
// - Up to two members describing the contents of the struct.
//
// TODO: Add your language's typed instruction representations here.
// See TinySwift compiler for reference implementation patterns.

namespace TinySwift::SemIR {

// A template for singleton types. Most uses will not add members, and so may
// apply a `using` alias.
template <InstKind::RawEnumType KindT, TemplateString IrName,
          ExprCategory Cat = ExprCategory::Value>
struct SingletonTypeInst {
  static constexpr auto Kind = InstKind::Make(KindT).Define<Parse::NoneNodeId>(
      InstKind::DefinitionInfo{.ir_name = IrName,
                               .expr_category = Cat,
                               .is_type = InstIsType::Always,
                               .constant_kind = InstConstantKind::Always});
  static constexpr auto TypeInstId = MakeSingletonTypeInstId<Kind>();

  SemIR::TypeId type_id;
};

// Used when a semantic error has been detected, and a SemIR InstId is still
// required.
struct ErrorInst : public SingletonTypeInst<InstKind::ErrorInst, "<error>",
                                            ExprCategory::Error> {
  static constexpr InstId InstId = TypeInstId;
  static constexpr auto ConstantId =
      ConstantId::ForConcreteConstant(TypeInstId);
  static constexpr auto TypeId = TypeId::ForTypeConstant(ConstantId);
};

// Consumes the initializer `rhs_id`, and performs assignment of `lhs_id`.
struct Assign {
  static constexpr auto Kind = InstKind::Assign.Define<Parse::NodeId>(
      {.ir_name = "assign", .constant_kind = InstConstantKind::Never});

  InstId lhs_id;
  InstId rhs_id;
};

// Control flow to branch to the target block.
struct Branch {
  static constexpr auto Kind = InstKind::Branch.Define<Parse::NodeId>(
      {.ir_name = "br",
       .constant_kind = InstConstantKind::Never,
       .terminator_kind = TerminatorKind::Terminator});

  LabelId target_id;
};

// Control flow to branch to the target block if `cond_id` is true.
struct BranchIf {
  static constexpr auto Kind = InstKind::BranchIf.Define<Parse::NodeId>(
      {.ir_name = "br",
       .constant_kind = InstConstantKind::Never,
       .terminator_kind = TerminatorKind::TerminatorSequence});

  LabelId target_id;
  InstId cond_id;
};

// Control flow to branch to the target block, passing an argument.
struct BranchWithArg {
  static constexpr auto Kind = InstKind::BranchWithArg.Define<Parse::NodeId>(
      {.ir_name = "br",
       .constant_kind = InstConstantKind::Never,
       .terminator_kind = TerminatorKind::Terminator});

  LabelId target_id;
  InstId arg_id;
};

// A call expression `callee(args)`.
struct Call {
  static constexpr auto Kind = InstKind::Call.Define<Parse::NodeId>(
      {.ir_name = "call",
       .expr_category = ComputedExprCategory::DependsOnOperands,
       .is_type = InstIsType::Maybe,
       .constant_kind = InstConstantKind::SymbolicOnly,
       .constant_needs_inst_id =
           InstConstantNeedsInstIdKind::DuringEvaluation});

  TypeId type_id;
  InstId callee_id;
  InstBlockId args_id;
};

// A return statement with no expression.
struct Return {
  static constexpr auto Kind = InstKind::Return.Define<Parse::NodeId>(
      {.ir_name = "return",
       .constant_kind = InstConstantKind::Never,
       .terminator_kind = TerminatorKind::Terminator});
};

// A `return expr;` statement.
struct ReturnExpr {
  static constexpr auto Kind = InstKind::ReturnExpr.Define<Parse::NodeId>(
      {.ir_name = "return",
       .constant_kind = InstConstantKind::Never,
       .terminator_kind = TerminatorKind::Terminator});

  InstId expr_id;
  DestInstId dest_id;
};

// The type of bool values.
using BoolType = SingletonTypeInst<InstKind::BoolType, "bool">;

// A primitive integer type.
struct IntType {
  static constexpr auto Kind = InstKind::IntType.Define<Parse::NoneNodeId>(
      {.ir_name = "int_type",
       .is_type = InstIsType::Always,
       .constant_kind = InstConstantKind::Conditional,
       .constant_needs_inst_id = InstConstantNeedsInstIdKind::DuringEvaluation,
       .deduce_through = true});

  TypeId type_id;
  IntKind int_kind;
  InstId bit_width_id;
};

// The type of a function.
struct FunctionType {
  static constexpr auto Kind =
      InstKind::FunctionType.Define<Parse::AnyFunctionDeclId>(
          {.ir_name = "fn_type",
           .is_type = InstIsType::Always,
           .constant_kind = InstConstantKind::WheneverPossible});

  TypeId type_id;
  FunctionId function_id;
  SpecificId specific_id;
};

// A pointer type `T*`.
struct PointerType {
  static constexpr auto Kind =
      InstKind::PointerType.Define<Parse::PostfixOperatorExprId>(
          {.ir_name = "ptr_type",
           .is_type = InstIsType::Always,
           .constant_kind = InstConstantKind::WheneverPossible,
           .deduce_through = true});

  TypeId type_id;
  TypeInstId pointee_id;
};

// Tracks expressions which are valid as types.
struct TypeType : public SingletonTypeInst<InstKind::TypeType, "type"> {
  static constexpr auto TypeId =
      TypeId::ForTypeConstant(ConstantId::ForConcreteConstant(TypeInstId));
};

// A literal bool value.
struct BoolLiteral {
  static constexpr auto Kind = InstKind::BoolLiteral.Define<Parse::NodeId>(
      {.ir_name = "bool_literal", .constant_kind = InstConstantKind::Always});

  TypeId type_id;
  BoolValue value;
};

// An integer value.
struct IntValue {
  static constexpr auto Kind = InstKind::IntValue.Define<Parse::NodeId>(
      {.ir_name = "int_value", .constant_kind = InstConstantKind::Always});

  TypeId type_id;
  IntId int_id;
};

// The integer literal type.
using IntLiteralType =
    SingletonTypeInst<InstKind::IntLiteralType, "Core.IntLiteral">;

// Storage for a `var` pattern.
struct VarStorage {
  static constexpr auto Kind = InstKind::VarStorage.Define<Parse::NodeId>(
      {.ir_name = "var",
       .expr_category = ExprCategory::DurableRef,
       .constant_kind = InstConstantKind::ConditionalUnique,
       .constant_needs_inst_id = InstConstantNeedsInstIdKind::Permanent,
       .has_cleanup = true});

  TypeId type_id;
  AbsoluteInstId pattern_id;
};

// A name reference.
struct NameRef {
  static constexpr auto Kind = InstKind::NameRef.Define<Parse::NodeId>(
      {.ir_name = "name_ref",
       .expr_category = ComputedExprCategory::SameAsSecondOperand});

  TypeId type_id;
  NameId name_id;
  InstId value_id;
};

// A namespace declaration.
struct Namespace {
  static constexpr auto Kind =
      InstKind::Namespace.Define<Parse::NodeId>(
          {.ir_name = "namespace",
           .expr_category = ExprCategory::NotExpr,
           .constant_kind = InstConstantKind::AlwaysUnique});
  // The file's package namespace, immediately after singletons.
  static constexpr InstId PackageInstId = InstId(SingletonInstKinds.size());

  TypeId type_id;
  NameScopeId name_scope_id;
  AbsoluteInstId import_id;
};

// The type of namespace and package names.
using NamespaceType = SingletonTypeInst<InstKind::NamespaceType, "<namespace>">;

// A name-binding declaration (let or var).
struct NameBindingDecl {
  static constexpr auto Kind = InstKind::NameBindingDecl.Define<Parse::NodeId>(
      {.ir_name = "name_binding_decl",
       .expr_category = ExprCategory::NotExpr,
       .constant_kind = InstConstantKind::Never});

  InstBlockId pattern_block_id;
};

// Binds a name as a value expression.
struct ValueBinding {
  static constexpr auto Kind = InstKind::ValueBinding.Define<Parse::NodeId>(
      {.ir_name = "value_binding",
       .constant_kind = InstConstantKind::Indirect});

  TypeId type_id;
  EntityNameId entity_name_id;
  InstId value_id;
};

// Represents a value binding pattern.
struct ValueBindingPattern {
  static constexpr auto Kind =
      InstKind::ValueBindingPattern.Define<Parse::NodeId>(
          {.ir_name = "value_binding_pattern",
           .expr_category = ExprCategory::Pattern,
           .constant_kind = InstConstantKind::AlwaysUnique,
           .is_lowered = false});

  TypeId type_id;
  EntityNameId entity_name_id;
};

// A by-value `Call` parameter.
struct ValueParam {
  static constexpr auto Kind = InstKind::ValueParam.Define<Parse::NodeId>(
      {.ir_name = "value_param", .constant_kind = InstConstantKind::Never});

  TypeId type_id;
  CallParamIndex index;
  NameId pretty_name_id;
};

// A pattern that represents a by-value `Call` parameter.
struct ValueParamPattern {
  static constexpr auto Kind =
      InstKind::ValueParamPattern.Define<Parse::NodeId>(
          {.ir_name = "value_param_pattern",
           .expr_category = ExprCategory::Pattern,
           .constant_kind = InstConstantKind::AlwaysUnique,
           .is_lowered = false});

  TypeId type_id;
  InstId subpattern_id;
  CallParamIndex index;
};

// A function declaration.
struct FunctionDecl {
  static constexpr auto Kind =
      InstKind::FunctionDecl.Define<Parse::AnyFunctionDeclId>(
          {.ir_name = "fn_decl",
           .expr_category = ExprCategory::NotExpr,
           .is_lowered = false});

  TypeId type_id;
  FunctionId function_id;
  DeclInstBlockId decl_block_id;
};

// Reads an argument from `BranchWithArg`.
struct BlockArg {
  static constexpr auto Kind = InstKind::BlockArg.Define<Parse::NodeId>(
      {.ir_name = "block_arg", .constant_kind = InstConstantKind::Never});

  TypeId type_id;
  LabelId block_id;
};

// Splices a block into the current location.
struct SpliceBlock {
  static constexpr auto Kind = InstKind::SpliceBlock.Define<Parse::NodeId>(
      {.ir_name = "splice_block",
       .expr_category = ComputedExprCategory::SameAsSecondOperand});

  TypeId type_id;
  AbsoluteInstBlockId block_id;
  InstId result_id;
};

// An `import` declaration.
struct ImportDecl {
  static constexpr auto Kind =
      InstKind::ImportDecl.Define<Parse::ImportDeclId>(
          {.ir_name = "import",
           .constant_kind = InstConstantKind::Never,
           .is_lowered = false});

  NameId package_id;
};

// Records that a type conversion was done.
struct Converted {
  static constexpr auto Kind = InstKind::Converted.Define<Parse::NodeId>(
      {.ir_name = "converted",
       .expr_category = ComputedExprCategory::SameAsSecondOperand});

  TypeId type_id;
  AbsoluteInstId original_id;
  InstId result_id;
};

// The type of string values.
using StringType = SingletonTypeInst<InstKind::StringType, "String">;

// A string literal value.
struct StringLiteral {
  static constexpr auto Kind = InstKind::StringLiteral.Define<Parse::NodeId>(
      {.ir_name = "string_literal", .constant_kind = InstConstantKind::Always});

  TypeId type_id;
  StringLiteralValueId string_id;
};

// The type of Float values.
using FloatType = SingletonTypeInst<InstKind::FloatType, "Float">;

// The type of Double values.
using DoubleType = SingletonTypeInst<InstKind::DoubleType, "Double">;

// A floating-point value.
struct FloatValue {
  static constexpr auto Kind = InstKind::FloatValue.Define<Parse::NodeId>(
      {.ir_name = "float_value", .constant_kind = InstConstantKind::Always});

  TypeId type_id;
  FloatId float_id;
};

// An optional type `T?`.
struct OptionalType {
  static constexpr auto Kind = InstKind::OptionalType.Define<Parse::NodeId>(
      {.ir_name = "optional_type",
       .is_type = InstIsType::Always,
       .constant_kind = InstConstantKind::WheneverPossible});

  TypeId type_id;
  TypeInstId inner_type_id;
};

// An optional none value (`nil`).
struct OptionalNone {
  static constexpr auto Kind = InstKind::OptionalNone.Define<Parse::NodeId>(
      {.ir_name = "optional_none",
       .constant_kind = InstConstantKind::Always});

  TypeId type_id;
};

// An optional some value (wrapping a value).
struct OptionalSome {
  static constexpr auto Kind = InstKind::OptionalSome.Define<Parse::NodeId>(
      {.ir_name = "optional_some"});

  TypeId type_id;
  InstId value_id;
};

// Integer arithmetic operations.
struct IntAdd {
  static constexpr auto Kind = InstKind::IntAdd.Define<Parse::NodeId>(
      {.ir_name = "int_add"});
  TypeId type_id;
  InstId lhs_id;
  InstId rhs_id;
};

struct IntSub {
  static constexpr auto Kind = InstKind::IntSub.Define<Parse::NodeId>(
      {.ir_name = "int_sub"});
  TypeId type_id;
  InstId lhs_id;
  InstId rhs_id;
};

struct IntMul {
  static constexpr auto Kind = InstKind::IntMul.Define<Parse::NodeId>(
      {.ir_name = "int_mul"});
  TypeId type_id;
  InstId lhs_id;
  InstId rhs_id;
};

struct IntDiv {
  static constexpr auto Kind = InstKind::IntDiv.Define<Parse::NodeId>(
      {.ir_name = "int_div"});
  TypeId type_id;
  InstId lhs_id;
  InstId rhs_id;
};

struct IntMod {
  static constexpr auto Kind = InstKind::IntMod.Define<Parse::NodeId>(
      {.ir_name = "int_mod"});
  TypeId type_id;
  InstId lhs_id;
  InstId rhs_id;
};

struct IntNegate {
  static constexpr auto Kind = InstKind::IntNegate.Define<Parse::NodeId>(
      {.ir_name = "int_negate"});
  TypeId type_id;
  InstId operand_id;
};

// Integer comparison operations.
struct IntEq {
  static constexpr auto Kind = InstKind::IntEq.Define<Parse::NodeId>(
      {.ir_name = "int_eq"});
  TypeId type_id;
  InstId lhs_id;
  InstId rhs_id;
};

struct IntNeq {
  static constexpr auto Kind = InstKind::IntNeq.Define<Parse::NodeId>(
      {.ir_name = "int_neq"});
  TypeId type_id;
  InstId lhs_id;
  InstId rhs_id;
};

struct IntLess {
  static constexpr auto Kind = InstKind::IntLess.Define<Parse::NodeId>(
      {.ir_name = "int_less"});
  TypeId type_id;
  InstId lhs_id;
  InstId rhs_id;
};

struct IntGreater {
  static constexpr auto Kind = InstKind::IntGreater.Define<Parse::NodeId>(
      {.ir_name = "int_greater"});
  TypeId type_id;
  InstId lhs_id;
  InstId rhs_id;
};

struct IntLessEq {
  static constexpr auto Kind = InstKind::IntLessEq.Define<Parse::NodeId>(
      {.ir_name = "int_less_eq"});
  TypeId type_id;
  InstId lhs_id;
  InstId rhs_id;
};

struct IntGreaterEq {
  static constexpr auto Kind = InstKind::IntGreaterEq.Define<Parse::NodeId>(
      {.ir_name = "int_greater_eq"});
  TypeId type_id;
  InstId lhs_id;
  InstId rhs_id;
};

// Boolean operations.
struct BoolNot {
  static constexpr auto Kind = InstKind::BoolNot.Define<Parse::NodeId>(
      {.ir_name = "bool_not"});
  TypeId type_id;
  InstId operand_id;
};

struct BoolAnd {
  static constexpr auto Kind = InstKind::BoolAnd.Define<Parse::NodeId>(
      {.ir_name = "bool_and"});
  TypeId type_id;
  InstId lhs_id;
  InstId rhs_id;
};

struct BoolOr {
  static constexpr auto Kind = InstKind::BoolOr.Define<Parse::NodeId>(
      {.ir_name = "bool_or"});
  TypeId type_id;
  InstId lhs_id;
  InstId rhs_id;
};

// A struct type declaration.
struct StructType {
  static constexpr auto Kind = InstKind::StructType.Define<Parse::NodeId>(
      {.ir_name = "struct_type",
       .is_type = InstIsType::Always,
       .constant_kind = InstConstantKind::AlwaysUnique});

  TypeId type_id;
  NameScopeId name_scope_id;
};

// A struct initialization expression.
struct StructInit {
  static constexpr auto Kind = InstKind::StructInit.Define<Parse::NodeId>(
      {.ir_name = "struct_init"});

  TypeId type_id;
  InstBlockId args_id;
};

// A field access expression `base.field`.
struct FieldAccess {
  static constexpr auto Kind = InstKind::FieldAccess.Define<Parse::NodeId>(
      {.ir_name = "field_access"});

  TypeId type_id;
  InstId base_id;
  ElementIndex index;
};

// A class type declaration.
struct ClassType {
  static constexpr auto Kind = InstKind::ClassType.Define<Parse::NodeId>(
      {.ir_name = "class_type",
       .is_type = InstIsType::Always,
       .constant_kind = InstConstantKind::AlwaysUnique});

  TypeId type_id;
  NameScopeId name_scope_id;
};

// These concepts are an implementation detail of the library, not public API.
namespace Internal {

// HasNodeId is true if T has an associated parse node.
template <typename T>
concept HasNodeId =
    !std::same_as<typename decltype(T::Kind)::TypedNodeId, Parse::NoneNodeId>;

// HasUntypedNodeId is true if T has an associated parse node which can be any
// kind of node.
template <typename T>
concept HasUntypedNodeId =
    std::same_as<typename decltype(T::Kind)::TypedNodeId, Parse::NodeId>;

// HasKindMemberAsField<T> is true if T has a `InstKind kind` field.
template <typename T>
concept HasKindMemberAsField = std::same_as<decltype(T::kind), InstKind>;

// HasTypeIdMember<T> is true if T has a `TypeId type_id` field.
template <typename T>
concept HasTypeIdMember = std::same_as<decltype(T::type_id), TypeId>;

}  // namespace Internal

}  // namespace TinySwift::SemIR

#endif  // TINYSWIFT_TOOLCHAIN_SEM_IR_TYPED_INSTS_H_
