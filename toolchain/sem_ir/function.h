// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef TINYSWIFT_TOOLCHAIN_SEM_IR_FUNCTION_H_
#define TINYSWIFT_TOOLCHAIN_SEM_IR_FUNCTION_H_

#include "toolchain/base/value_store.h"
#include "toolchain/sem_ir/ids.h"
#include "toolchain/sem_ir/typed_insts.h"

// TODO: Implement your language's function representation here.
// See TinySwift compiler for reference implementation patterns.

namespace TinySwift::SemIR {

class File;

// M74: Access control levels for declarations.
enum class AccessLevel : uint8_t { Public, Internal, Private };

// A function declaration and definition.
struct Function : public Printable<Function> {
  // The function's name.
  NameId name_id = NameId::None;

  // M74: Access control level (default: Internal).
  AccessLevel access_level = AccessLevel::Internal;

  // The enclosing scope.
  NameScopeId parent_scope_id = NameScopeId::None;

  // The declaration block of pattern instructions.
  DeclInstBlockId decl_block_id = DeclInstBlockId::None;

  // The call parameter patterns.
  InstBlockId call_param_patterns_id = InstBlockId::None;

  // The call parameter instructions.
  InstBlockId call_params_id = InstBlockId::None;

  // The return type instruction.
  TypeInstId return_type_inst_id = TypeInstId::None;

  // Whether this is a static method (no implicit `self` parameter).
  // Set for functions declared with the `static` modifier inside a type.
  bool is_static = false;

  // Whether this is a mutating method (self passed by pointer/inout).
  // Set for functions declared with the `mutating` modifier (M56) or for
  // all class instance methods (M54, which have implicit reference semantics).
  bool is_mutating = false;

  // Whether this function is declared with `throws`.
  bool is_throwing = false;

  // M75: Whether this function is an @extern("C") import.
  bool is_extern_c = false;

  // M75: The C symbol name for @extern("C") functions (empty = use name_id).
  std::string extern_name;

  // M76: Whether this function is exported with @cdecl.
  bool is_cdecl = false;

  // M76: The exported C symbol name for @cdecl functions.
  std::string cdecl_name;

  // M98: Whether this function is a generator (returns Generator<T>).
  bool is_generator = false;

  // M98: The element type T for Generator<T> functions.
  SemIR::TypeId generator_element_type_id = SemIR::TypeId::None;

  // M100: Whether this function is async.
  bool is_async = false;

  // M112: Whether this function is comptime-only.
  bool is_comptime = false;

  // M66-M68: Generics support.
  // Whether this function is a generic template (body not yet specialized).
  bool is_generic_template = false;

  // Generic parameter info: name + optional constraint.
  struct GenericParamInfo {
    SemIR::NameId name_id = SemIR::NameId::None;
    SemIR::NameId constraint_name_id = SemIR::NameId::None;
  };
  llvm::SmallVector<GenericParamInfo> generic_params;

  // Parse tree nodes for re-checking during specialization.
  Parse::NodeId template_node_id = Parse::NodeId::None;       // FunctionDefinition
  Parse::NodeId template_sig_node_id = Parse::NodeId::None;    // FunctionDefinitionStart

  // M73: Which file this function template was defined in (for cross-file
  // generic instantiation). Only meaningful when is_generic_template is true.
  CheckIRId source_check_ir_id = CheckIRId(0);

  // Default value parse nodes for parameters, indexed by param position
  // (0-based, after self is excluded). NodeId::None means no default.
  llvm::SmallVector<Parse::NodeId> param_default_nodes = {};

  // A list of the statically reachable code blocks in the body of the
  // function, in lexical order. The first block is the entry block. This will
  // be empty for declarations that don't have a visible definition.
  llvm::SmallVector<InstBlockId> body_block_ids = {};

  auto Print(llvm::raw_ostream& out) const -> void {
    out << "{name: " << name_id;
    if (call_param_patterns_id.has_value()) {
      out << ", call_param_patterns_id: " << call_param_patterns_id;
    }
    if (call_params_id.has_value()) {
      out << ", call_params_id: " << call_params_id;
    }
    if (return_type_inst_id.has_value()) {
      out << ", return_type_inst_id: " << return_type_inst_id;
    }
    if (!body_block_ids.empty()) {
      out << llvm::formatv(
          ", body: [{0}]",
          llvm::make_range(body_block_ids.begin(), body_block_ids.end()));
    }
    out << "}";
  }

  // Gets the declared return type, or None if no return type was specified.
  auto GetDeclaredReturnType(const File& file,
                             SpecificId specific_id = SpecificId::None) const
      -> TypeId;
};

using FunctionStore = ValueStore<FunctionId, Function, Tag<CheckIRId>>;

// Information about a callee that's `ErrorInst`.
struct CalleeError {};

// Information about a callee that's a function.
struct CalleeFunction {
  // The function.
  FunctionId function_id;
  // The specific that contains the function.
  SpecificId enclosing_specific_id;
  // The specific for the callee itself, in a resolved call.
  SpecificId resolved_specific_id;
  // The bound `self` parameter. `None` if not a method.
  InstId self_id;
};

// Information about a callee that may be a generic type, or could be an
// invalid callee.
struct CalleeNonFunction {};

// A variant combining the callee forms.
using Callee = std::variant<CalleeError, CalleeFunction, CalleeNonFunction>;

// Returns information for the function corresponding to callee_id.
auto GetCallee(const File& sem_ir, InstId callee_id,
               SpecificId caller_specific_id = SpecificId::None) -> Callee;

// Like `GetCallee`, but restricts to the `Function` callee kind.
auto GetCalleeAsFunction(const File& sem_ir, InstId callee_id,
                         SpecificId caller_specific_id = SpecificId::None)
    -> CalleeFunction;

}  // namespace TinySwift::SemIR

#endif  // TINYSWIFT_TOOLCHAIN_SEM_IR_FUNCTION_H_
