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

// Float arithmetic operations.
struct FloatAdd {
  static constexpr auto Kind = InstKind::FloatAdd.Define<Parse::NodeId>(
      {.ir_name = "float_add"});
  TypeId type_id;
  InstId lhs_id;
  InstId rhs_id;
};

struct FloatSub {
  static constexpr auto Kind = InstKind::FloatSub.Define<Parse::NodeId>(
      {.ir_name = "float_sub"});
  TypeId type_id;
  InstId lhs_id;
  InstId rhs_id;
};

struct FloatMul {
  static constexpr auto Kind = InstKind::FloatMul.Define<Parse::NodeId>(
      {.ir_name = "float_mul"});
  TypeId type_id;
  InstId lhs_id;
  InstId rhs_id;
};

struct FloatDiv {
  static constexpr auto Kind = InstKind::FloatDiv.Define<Parse::NodeId>(
      {.ir_name = "float_div"});
  TypeId type_id;
  InstId lhs_id;
  InstId rhs_id;
};

struct FloatNegate {
  static constexpr auto Kind = InstKind::FloatNegate.Define<Parse::NodeId>(
      {.ir_name = "float_negate"});
  TypeId type_id;
  InstId operand_id;
};

// Float comparison operations.
struct FloatEq {
  static constexpr auto Kind = InstKind::FloatEq.Define<Parse::NodeId>(
      {.ir_name = "float_eq"});
  TypeId type_id;
  InstId lhs_id;
  InstId rhs_id;
};

struct FloatNeq {
  static constexpr auto Kind = InstKind::FloatNeq.Define<Parse::NodeId>(
      {.ir_name = "float_neq"});
  TypeId type_id;
  InstId lhs_id;
  InstId rhs_id;
};

struct FloatLess {
  static constexpr auto Kind = InstKind::FloatLess.Define<Parse::NodeId>(
      {.ir_name = "float_less"});
  TypeId type_id;
  InstId lhs_id;
  InstId rhs_id;
};

struct FloatGreater {
  static constexpr auto Kind = InstKind::FloatGreater.Define<Parse::NodeId>(
      {.ir_name = "float_greater"});
  TypeId type_id;
  InstId lhs_id;
  InstId rhs_id;
};

struct FloatLessEq {
  static constexpr auto Kind = InstKind::FloatLessEq.Define<Parse::NodeId>(
      {.ir_name = "float_less_eq"});
  TypeId type_id;
  InstId lhs_id;
  InstId rhs_id;
};

struct FloatGreaterEq {
  static constexpr auto Kind = InstKind::FloatGreaterEq.Define<Parse::NodeId>(
      {.ir_name = "float_greater_eq"});
  TypeId type_id;
  InstId lhs_id;
  InstId rhs_id;
};

// String concatenation.
struct StringConcat {
  static constexpr auto Kind = InstKind::StringConcat.Define<Parse::NodeId>(
      {.ir_name = "string_concat"});
  TypeId type_id;
  InstId lhs_id;
  InstId rhs_id;
};

// Explicit type conversions.
struct IntToFloat {
  static constexpr auto Kind = InstKind::IntToFloat.Define<Parse::NodeId>(
      {.ir_name = "int_to_float"});
  TypeId type_id;
  InstId operand_id;
};

struct FloatToInt {
  static constexpr auto Kind = InstKind::FloatToInt.Define<Parse::NodeId>(
      {.ir_name = "float_to_int"});
  TypeId type_id;
  InstId operand_id;
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

// Address of a struct field (for lvalue member assignment, e.g. self.x = v).
// base_id must be a VarStorage (struct alloca). Produces a pointer to the field.
struct FieldAddr {
  static constexpr auto Kind = InstKind::FieldAddr.Define<Parse::NodeId>(
      {.ir_name = "field_addr",
       .expr_category = ExprCategory::Value,
       .constant_kind = InstConstantKind::Never});

  TypeId type_id;   // type of the field (not a pointer type — pointer inferred at lower)
  InstId base_id;   // VarStorage (struct alloca)
  ElementIndex index;  // field index
};

// A struct field descriptor.
struct StructField {
  static constexpr auto Kind = InstKind::StructField.Define<Parse::NodeId>(
      {.ir_name = "struct_field",
       .expr_category = ExprCategory::NotExpr,
       .constant_kind = InstConstantKind::Never,
       .is_lowered = false});

  TypeId type_id;
  NameId name_id;
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

// An enum type declaration.
struct EnumDecl {
  static constexpr auto Kind = InstKind::EnumDecl.Define<Parse::NodeId>(
      {.ir_name = "enum_decl",
       .is_type = InstIsType::Always,
       .constant_kind = InstConstantKind::AlwaysUnique});

  TypeId type_id;
  NameScopeId name_scope_id;
  InstBlockId cases_id;
};

// An enum case (no payload).
struct EnumCase {
  static constexpr auto Kind = InstKind::EnumCase.Define<Parse::NodeId>(
      {.ir_name = "enum_case",
       .constant_kind = InstConstantKind::Always});

  TypeId type_id;
  NameId name_id;
  ElementIndex discriminant;
};

// An enum case with associated payload value (e.g., `case success(Int)`).
// The case name is NOT stored here — it is registered in the enum's name scope
// by the check phase. The payload type is stored as an InstId (TypeInstId).
// Layout: type_id (enum type) + discriminant + payload_type_id.
struct EnumCaseWithPayload {
  static constexpr auto Kind =
      InstKind::EnumCaseWithPayload.Define<Parse::NodeId>(
          {.ir_name = "enum_case_payload",
           .constant_kind = InstConstantKind::Always});

  TypeId type_id;             // The enum type this case belongs to.
  ElementIndex discriminant;  // Case discriminant (integer tag).
  InstId payload_type_id;     // TypeInstId of the associated value type.
};

// Construct an enum value.
struct EnumInit {
  static constexpr auto Kind = InstKind::EnumInit.Define<Parse::NodeId>(
      {.ir_name = "enum_init"});

  TypeId type_id;
  InstId case_id;
};

// Extract the discriminant tag from an enum value.
struct EnumDiscriminant {
  static constexpr auto Kind =
      InstKind::EnumDiscriminant.Define<Parse::NodeId>(
          {.ir_name = "enum_discriminant"});

  TypeId type_id;
  InstId enum_id;
};

// Extract the payload from an enum value given a known discriminant.
struct EnumPayload {
  static constexpr auto Kind = InstKind::EnumPayload.Define<Parse::NodeId>(
      {.ir_name = "enum_payload"});

  TypeId type_id;
  InstId enum_id;
};

// A tuple type.
struct TupleType {
  static constexpr auto Kind = InstKind::TupleType.Define<Parse::NodeId>(
      {.ir_name = "tuple_type",
       .is_type = InstIsType::Always,
       .constant_kind = InstConstantKind::WheneverPossible});

  TypeId type_id;
  InstBlockId element_types_id;
};

// A tuple initialization expression.
struct TupleInit {
  static constexpr auto Kind = InstKind::TupleInit.Define<Parse::NodeId>(
      {.ir_name = "tuple_init"});

  TypeId type_id;
  InstBlockId elements_id;
};

// A tuple element access.
struct TupleAccess {
  static constexpr auto Kind = InstKind::TupleAccess.Define<Parse::NodeId>(
      {.ir_name = "tuple_access"});

  TypeId type_id;
  InstId tuple_id;
  ElementIndex index;
};

// A closure expression.
struct ClosureExpr {
  static constexpr auto Kind = InstKind::ClosureExpr.Define<Parse::NodeId>(
      {.ir_name = "closure_expr"});

  TypeId type_id;
  FunctionId function_id;
  InstBlockId captures_id;
};

// A reference to a captured variable.
struct CaptureRef {
  static constexpr auto Kind = InstKind::CaptureRef.Define<Parse::NodeId>(
      {.ir_name = "capture_ref"});

  TypeId type_id;
  InstId captured_inst_id;
};

// A switch instruction (multi-way branch).
struct SwitchInst {
  static constexpr auto Kind = InstKind::SwitchInst.Define<Parse::NodeId>(
      {.ir_name = "switch",
       .constant_kind = InstConstantKind::Never,
       .terminator_kind = TerminatorKind::Terminator});

  InstId scrutinee_id;
  InstBlockId case_blocks_id;
};

// A single case pattern in a switch.
struct CasePattern {
  static constexpr auto Kind = InstKind::CasePattern.Define<Parse::NodeId>(
      {.ir_name = "case_pattern",
       .constant_kind = InstConstantKind::Never});

  TypeId type_id;
  InstId pattern_id;
  InstBlockId body_id;
};

// A bound method: a function reference tied to a base object.
// Created when `p.method` is resolved and the member is a FunctionDecl.
// In HandleCallExpr, this is unwrapped to emit Call{callee=FunctionDecl, args=[self,...]}
struct BoundMethod {
  static constexpr auto Kind = InstKind::BoundMethod.Define<Parse::NodeId>(
      {.ir_name = "bound_method",
       .expr_category = ExprCategory::Value,
       .constant_kind = InstConstantKind::AlwaysUnique});

  TypeId type_id;     // placeholder — use SemIR::TypeType::TypeId
  InstId base_id;     // the receiver object (e.g., `p`)
  InstId function_id; // the FunctionDecl instruction for the method
};

// A computed property declaration (getter-only property in a struct/class).
// Registered in the type scope under the property name. When a member access
// resolves to this instruction, HandleMemberAccessExpr auto-calls the getter.
struct ComputedPropertyDecl {
  static constexpr auto Kind =
      InstKind::ComputedPropertyDecl.Define<Parse::NodeId>(
          {.ir_name = "computed_property_decl",
           .expr_category = ExprCategory::Value,
           .constant_kind = InstConstantKind::AlwaysUnique});

  TypeId type_id;     // the property's value type (e.g., Int)
  InstId getter_id;   // the FunctionDecl InstId for the synthetic getter
};

// An array literal initialization.
// type_id is the element type (e.g., Int for [5, 10, 15]).
// elements_id holds the InstIds of the element values.
struct ArrayLiteralInit {
  static constexpr auto Kind = InstKind::ArrayLiteralInit.Define<Parse::NodeId>(
      {.ir_name = "array_literal_init"});

  TypeId type_id;          // element type
  InstBlockId elements_id; // element values
};

// An array subscript access: arr[index].
// type_id is the element type.
// array_id is the array value (alloca pointer from ArrayLiteralInit).
// index_id is the integer index.
struct ArrayAccess {
  static constexpr auto Kind = InstKind::ArrayAccess.Define<Parse::NodeId>(
      {.ir_name = "array_access"});

  TypeId type_id;  // element type
  InstId array_id;
  InstId index_id;
};

// An array element address: &arr[index] (for lvalue assignment).
// type_id is the element type.
// array_id is the pointer to the array (from VarStorage/NameRef for var array).
// index_id is the integer index.
struct ArrayElementAddr {
  static constexpr auto Kind = InstKind::ArrayElementAddr.Define<Parse::NodeId>(
      {.ir_name = "array_element_addr"});

  TypeId type_id;  // element type
  InstId array_id;
  InstId index_id;
};

// Bitwise integer binary operations.
struct IntBitwiseAnd {
  static constexpr auto Kind = InstKind::IntBitwiseAnd.Define<Parse::NodeId>(
      {.ir_name = "int_bitwise_and"});
  TypeId type_id;
  InstId lhs_id;
  InstId rhs_id;
};

struct IntBitwiseOr {
  static constexpr auto Kind = InstKind::IntBitwiseOr.Define<Parse::NodeId>(
      {.ir_name = "int_bitwise_or"});
  TypeId type_id;
  InstId lhs_id;
  InstId rhs_id;
};

struct IntBitwiseXor {
  static constexpr auto Kind = InstKind::IntBitwiseXor.Define<Parse::NodeId>(
      {.ir_name = "int_bitwise_xor"});
  TypeId type_id;
  InstId lhs_id;
  InstId rhs_id;
};

struct IntBitwiseNot {
  static constexpr auto Kind = InstKind::IntBitwiseNot.Define<Parse::NodeId>(
      {.ir_name = "int_bitwise_not"});
  TypeId type_id;
  InstId operand_id;
};

struct IntShiftLeft {
  static constexpr auto Kind = InstKind::IntShiftLeft.Define<Parse::NodeId>(
      {.ir_name = "int_shift_left"});
  TypeId type_id;
  InstId lhs_id;
  InstId rhs_id;
};

struct IntShiftRight {
  static constexpr auto Kind = InstKind::IntShiftRight.Define<Parse::NodeId>(
      {.ir_name = "int_shift_right"});
  TypeId type_id;
  InstId lhs_id;
  InstId rhs_id;
};

// M38: Convert an integer/bool value to a string representation.
struct IntToString {
  static constexpr auto Kind = InstKind::IntToString.Define<Parse::NodeId>(
      {.ir_name = "int_to_string"});
  TypeId type_id;      // String TypeId
  InstId operand_id;   // Int/Bool value to convert
};

// M41: Throw an error value (terminator).
struct ThrowValue {
  static constexpr auto Kind = InstKind::ThrowValue.Define<Parse::NodeId>(
      {.ir_name = "throw_value",
       .constant_kind = InstConstantKind::Never,
       .terminator_kind = TerminatorKind::Terminator});
  InstId error_id;
};

// M42: Dictionary initialization `["k": v, ...]`.
// entries_id stores interleaved [key0, val0, key1, val1, ...].
struct DictInit {
  static constexpr auto Kind = InstKind::DictInit.Define<Parse::NodeId>(
      {.ir_name = "dict_init"});
  TypeId type_id;           // result type (ptr to dict)
  InstBlockId entries_id;   // interleaved [k0, v0, k1, v1, ...]
};

// M42: Dictionary subscript access `d["key"]` → Optional<V>.
struct DictAccess {
  static constexpr auto Kind = InstKind::DictAccess.Define<Parse::NodeId>(
      {.ir_name = "dict_access"});
  TypeId type_id;    // Optional<ValueType>
  InstId dict_id;
  InstId key_id;
};

// M40: An `inout` parameter (pointer ABI, passed by address).
struct InoutParam {
  static constexpr auto Kind = InstKind::InoutParam.Define<Parse::NodeId>(
      {.ir_name = "inout_param"});
  TypeId type_id;            // pointed-to type (e.g. Int)
  CallParamIndex index;
  NameId name_id;
};

// M40: Address-of expression `&varName` — produces a pointer to storage.
struct AddressOf {
  static constexpr auto Kind = InstKind::AddressOf.Define<Parse::NodeId>(
      {.ir_name = "address_of",
       .constant_kind = InstConstantKind::Never});
  TypeId type_id;      // ErrorInst::TypeId (carrier; pointer type inferred)
  InstId operand_id;   // The VarStorage InstId
};

// M44: print() built-in — call __tinyswift_print_int or __tinyswift_print_string
// at runtime. type_id is ErrorInst::TypeId (void side-effect).
struct PrintValue {
  static constexpr auto Kind = InstKind::PrintValue.Define<Parse::NodeId>(
      {.ir_name = "print_value", .constant_kind = InstConstantKind::Never});
  TypeId type_id;   // ErrorInst::TypeId (void)
  InstId arg_id;
};

// M45: String equality / inequality comparison.
struct StringEq {
  static constexpr auto Kind = InstKind::StringEq.Define<Parse::NodeId>(
      {.ir_name = "string_eq"});
  TypeId type_id;  // Bool
  InstId lhs_id;
  InstId rhs_id;
};

struct StringNeq {
  static constexpr auto Kind = InstKind::StringNeq.Define<Parse::NodeId>(
      {.ir_name = "string_neq"});
  TypeId type_id;  // Bool
  InstId lhs_id;
  InstId rhs_id;
};

// M49: Get the length of a String value as an Int.
struct StringLen {
  static constexpr auto Kind = InstKind::StringLen.Define<Parse::NodeId>(
      {.ir_name = "string_len"});
  TypeId type_id;    // Int
  InstId operand_id; // the String value
};

// M61: Pending string method call — resolved in HandleCallExpr.
// type_id is placeholder (ErrorInst::TypeId); real result type assigned by HandleCallExpr.
struct StringMethodRef {
  static constexpr auto Kind = InstKind::StringMethodRef.Define<Parse::NodeId>(
      {.ir_name = "string_method_ref"});
  TypeId type_id;        // placeholder
  InstId str_id;         // the String value
  NameId method_name_id; // which method (uppercased, hasPrefix, etc.)
};

// M61: Zero-arg string transformations.
struct StringUppercased {
  static constexpr auto Kind = InstKind::StringUppercased.Define<Parse::NodeId>(
      {.ir_name = "string_uppercased"});
  TypeId type_id;    // String
  InstId str_id;
};
struct StringLowercased {
  static constexpr auto Kind = InstKind::StringLowercased.Define<Parse::NodeId>(
      {.ir_name = "string_lowercased"});
  TypeId type_id;    // String
  InstId str_id;
};
struct StringTrimmed {
  static constexpr auto Kind = InstKind::StringTrimmed.Define<Parse::NodeId>(
      {.ir_name = "string_trimmed"});
  TypeId type_id;    // String
  InstId str_id;
};

// M61: String predicate methods (one arg).
struct StringHasPrefix {
  static constexpr auto Kind = InstKind::StringHasPrefix.Define<Parse::NodeId>(
      {.ir_name = "string_has_prefix"});
  TypeId type_id;  // Bool
  InstId str_id;
  InstId arg_id;   // prefix string
};
struct StringHasSuffix {
  static constexpr auto Kind = InstKind::StringHasSuffix.Define<Parse::NodeId>(
      {.ir_name = "string_has_suffix"});
  TypeId type_id;  // Bool
  InstId str_id;
  InstId arg_id;   // suffix string
};
struct StringContains {
  static constexpr auto Kind = InstKind::StringContains.Define<Parse::NodeId>(
      {.ir_name = "string_contains"});
  TypeId type_id;  // Bool
  InstId str_id;
  InstId arg_id;   // substring
};

// M65: Dynamic array creation (var arr: [T] = []).
// type_id is the pointer to the opaque array (ErrorInst::TypeId used as ptr marker).
struct DynamicArrayInit {
  static constexpr auto Kind = InstKind::DynamicArrayInit.Define<Parse::NodeId>(
      {.ir_name = "dynamic_array_init"});
  TypeId type_id;      // element type
};

// M65: arr.append(x) — side effect only.
struct DynamicArrayAppend {
  static constexpr auto Kind = InstKind::DynamicArrayAppend.Define<Parse::NodeId>(
      {.ir_name = "dynamic_array_append",
       .constant_kind = InstConstantKind::Never});
  TypeId type_id;    // ErrorInst::TypeId (void)
  InstId array_id;   // the dynamic array var
  InstId elem_id;    // element to append
};

// M65: arr.count — runtime length.
struct DynamicArrayCount {
  static constexpr auto Kind = InstKind::DynamicArrayCount.Define<Parse::NodeId>(
      {.ir_name = "dynamic_array_count"});
  TypeId type_id;   // Int
  InstId array_id;
};

// M65: arr[i] — runtime element access.
struct DynamicArrayAccess {
  static constexpr auto Kind = InstKind::DynamicArrayAccess.Define<Parse::NodeId>(
      {.ir_name = "dynamic_array_access"});
  TypeId type_id;    // element type
  InstId array_id;
  InstId index_id;
};

// M65: Pending dynamic array method call — resolved in HandleCallExpr.
// Used for arr.append(...) where arg is not yet known.
struct DynamicArrayMethodRef {
  static constexpr auto Kind = InstKind::DynamicArrayMethodRef.Define<Parse::NodeId>(
      {.ir_name = "dynamic_array_method_ref"});
  TypeId type_id;        // placeholder (ErrorInst::TypeId)
  InstId array_id;       // the dynamic array
  NameId method_name_id; // which method ("append" etc.)
};

// M77: UnsafeMutablePointer<T>.allocate(capacity:) → ptr.
// capacity_id is the Int capacity, elem_size is a compile-time constant.
struct UnsafePtrAllocate {
  static constexpr auto Kind = InstKind::UnsafePtrAllocate.Define<Parse::NodeId>(
      {.ir_name = "unsafe_ptr_allocate"});
  TypeId type_id;         // PointerType (result)
  InstId capacity_id;     // Int value for capacity
  InstId elem_size_id;    // Int constant for element size in bytes
};

// M77: ptr.deallocate() — free heap memory.
struct UnsafePtrDeallocate {
  static constexpr auto Kind = InstKind::UnsafePtrDeallocate.Define<Parse::NodeId>(
      {.ir_name = "unsafe_ptr_deallocate",
       .constant_kind = InstConstantKind::Never});
  TypeId type_id;    // ErrorInst::TypeId (void)
  InstId ptr_id;     // the pointer to deallocate
};

// M77: ptr[i] — read element at index.
struct UnsafePtrSubscript {
  static constexpr auto Kind = InstKind::UnsafePtrSubscript.Define<Parse::NodeId>(
      {.ir_name = "unsafe_ptr_subscript"});
  TypeId type_id;    // element type (Int)
  InstId ptr_id;
  InstId index_id;
};

// M77: ptr[i] address for write — GEP only, no load.
struct UnsafePtrSubscriptAddr {
  static constexpr auto Kind = InstKind::UnsafePtrSubscriptAddr.Define<Parse::NodeId>(
      {.ir_name = "unsafe_ptr_subscript_addr",
       .constant_kind = InstConstantKind::Never});
  TypeId type_id;    // element type (Int)
  InstId ptr_id;
  InstId index_id;
};

// M77: Pending method reference for ptr.allocate/deallocate.
struct UnsafePtrMethodRef {
  static constexpr auto Kind = InstKind::UnsafePtrMethodRef.Define<Parse::NodeId>(
      {.ir_name = "unsafe_ptr_method_ref"});
  TypeId type_id;           // placeholder (ErrorInst::TypeId)
  InstId ptr_id;            // the pointer (or type for static calls)
  NameId method_name_id;    // "allocate" or "deallocate"
};

// M78: Heap-allocate a class instance and store initial field values.
struct AllocClass {
  static constexpr auto Kind = InstKind::AllocClass.Define<Parse::NodeId>(
      {.ir_name = "alloc_class"});
  TypeId type_id;           // ClassType
  InstBlockId args_id;      // field values to store
};

// M78: Increment reference count.
struct Retain {
  static constexpr auto Kind = InstKind::Retain.Define<Parse::NodeId>(
      {.ir_name = "retain",
       .constant_kind = InstConstantKind::Never});
  TypeId type_id;           // ErrorInst::TypeId (void side-effect)
  InstId value_id;          // class-typed pointer to retain
};

// M78: Decrement reference count; free if zero.
struct Release {
  static constexpr auto Kind = InstKind::Release.Define<Parse::NodeId>(
      {.ir_name = "release",
       .constant_kind = InstConstantKind::Never});
  TypeId type_id;           // ErrorInst::TypeId (void side-effect)
  InstId value_id;          // class-typed pointer to release
  InstId deinit_id;         // FunctionDecl for deinit, or InstId::None
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
