// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "toolchain/tiny_sil/printer.h"

#include "llvm/Support/raw_ostream.h"

namespace TinySwift::TinySIL {

namespace {

auto PrintValue(llvm::raw_ostream& out, const SILValue& val) -> void {
  if (val.id < 0) {
    out << "undef";
  } else {
    out << "%" << val.id;
  }
}

auto PrintInstKindName(SILInstKind kind) -> const char* {
  switch (kind) {
    case SILInstKind::AllocStack: return "alloc_stack";
    case SILInstKind::DeallocStack: return "dealloc_stack";
    case SILInstKind::AllocBox: return "alloc_box";
    case SILInstKind::Load: return "load";
    case SILInstKind::Store: return "store";
    case SILInstKind::IntegerLiteral: return "integer_literal";
    case SILInstKind::FloatLiteral: return "float_literal";
    case SILInstKind::StringLiteral: return "string_literal";
    case SILInstKind::StructInst: return "struct";
    case SILInstKind::TupleInst: return "tuple";
    case SILInstKind::EnumInst: return "enum";
    case SILInstKind::StructExtract: return "struct_extract";
    case SILInstKind::StructElementAddr: return "struct_element_addr";
    case SILInstKind::TupleExtract: return "tuple_extract";
    case SILInstKind::TupleElementAddr: return "tuple_element_addr";
    case SILInstKind::FunctionRef: return "function_ref";
    case SILInstKind::Apply: return "apply";
    case SILInstKind::PartialApply: return "partial_apply";
    case SILInstKind::ReturnInst: return "return";
    case SILInstKind::Branch: return "br";
    case SILInstKind::CondBranch: return "cond_br";
    case SILInstKind::SwitchEnum: return "switch_enum";
    case SILInstKind::Unreachable: return "unreachable";
    case SILInstKind::ConvertFunction: return "convert_function";
    case SILInstKind::Upcast: return "upcast";
    case SILInstKind::UncheckedEnumData: return "unchecked_enum_data";
    case SILInstKind::CopyValue: return "copy_value";
    case SILInstKind::DestroyValue: return "destroy_value";
    case SILInstKind::BeginBorrow: return "begin_borrow";
    case SILInstKind::EndBorrow: return "end_borrow";
    case SILInstKind::DebugValue: return "debug_value";
    case SILInstKind::BuiltinInst: return "builtin";
  }
  return "<unknown>";
}

auto PrintInst(llvm::raw_ostream& out, const SILInstruction& inst) -> void {
  out << "  ";

  // Print result if it produces a value.
  if (inst.result.is_valid()) {
    PrintValue(out, inst.result);
    out << " = ";
  }

  out << PrintInstKindName(inst.kind);

  switch (inst.kind) {
    case SILInstKind::IntegerLiteral:
      out << " $Int, " << inst.literal_value;
      break;

    case SILInstKind::FloatLiteral:
      out << " $Double, " << inst.float_literal_value;
      break;

    case SILInstKind::StringLiteral:
      out << " utf8 \"" << inst.string_literal_value << "\"";
      break;

    case SILInstKind::FunctionRef:
      out << " @" << inst.function_name;
      break;

    case SILInstKind::Apply: {
      out << " ";
      PrintValue(out, inst.operands[0]);
      out << "(";
      for (size_t i = 0; i < inst.operand_list.size(); ++i) {
        if (i > 0) out << ", ";
        PrintValue(out, inst.operand_list[i]);
      }
      out << ")";
      break;
    }

    case SILInstKind::ReturnInst:
      out << " ";
      PrintValue(out, inst.operands[0]);
      break;

    case SILInstKind::Branch:
      out << " bb" << inst.target_block;
      if (!inst.branch_args.empty()) {
        out << "(";
        for (size_t i = 0; i < inst.branch_args.size(); ++i) {
          if (i > 0) out << ", ";
          PrintValue(out, inst.branch_args[i]);
        }
        out << ")";
      }
      break;

    case SILInstKind::CondBranch:
      out << " ";
      PrintValue(out, inst.operands[0]);
      out << ", bb" << inst.true_block << ", bb" << inst.false_block;
      break;

    case SILInstKind::AllocStack:
      out << " $type_" << inst.alloc_type.type_index;
      break;

    case SILInstKind::Load:
      out << " ";
      PrintValue(out, inst.operands[0]);
      break;

    case SILInstKind::Store:
      out << " ";
      PrintValue(out, inst.operands[0]);
      out << " to ";
      PrintValue(out, inst.operands[1]);
      break;

    case SILInstKind::StructExtract:
    case SILInstKind::TupleExtract:
      out << " ";
      PrintValue(out, inst.operands[0]);
      out << ", #" << inst.element_index;
      break;

    case SILInstKind::BuiltinInst:
      out << " \"" << inst.builtin_name << "\"(";
      for (int i = 0; i < inst.num_operands; ++i) {
        if (i > 0) out << ", ";
        PrintValue(out, inst.operands[i]);
      }
      out << ")";
      break;

    case SILInstKind::CopyValue:
    case SILInstKind::DestroyValue:
    case SILInstKind::BeginBorrow:
    case SILInstKind::EndBorrow:
      out << " ";
      PrintValue(out, inst.operands[0]);
      break;

    case SILInstKind::DebugValue:
      out << " ";
      PrintValue(out, inst.operands[0]);
      break;

    default:
      // Print operands generically.
      for (int i = 0; i < inst.num_operands; ++i) {
        out << " ";
        PrintValue(out, inst.operands[i]);
      }
      break;
  }

  out << "\n";
}

}  // namespace

auto PrintSILFunction(llvm::raw_ostream& out, const SILFunction& function)
    -> void {
  if (function.is_declaration) {
    out << "sil @" << function.name << " : $(...) -> ...\n\n";
    return;
  }

  out << "sil @" << function.name << " : $(";
  for (size_t i = 0; i < function.type.param_types.size(); ++i) {
    if (i > 0) out << ", ";
    out << "$type_" << function.type.param_types[i].type_index;
  }
  out << ") -> ";
  if (function.type.is_void_return) {
    out << "()";
  } else {
    out << "$type_" << function.type.return_type.type_index;
  }
  out << " {\n";

  for (auto& bb : function.blocks) {
    out << "bb" << bb->id;
    if (!bb->args.empty()) {
      out << "(";
      for (size_t i = 0; i < bb->args.size(); ++i) {
        if (i > 0) out << ", ";
        PrintValue(out, bb->args[i]);
      }
      out << ")";
    }
    out << ":\n";

    for (auto& inst : bb->insts) {
      PrintInst(out, *inst);
    }
  }

  out << "} // end sil function '" << function.name << "'\n\n";
}

auto PrintSILModule(llvm::raw_ostream& out, const SILModule& module) -> void {
  out << "// SIL Module: " << module.name << "\n\n";

  for (auto& fn : module.functions) {
    PrintSILFunction(out, *fn);
  }
}

}  // namespace TinySwift::TinySIL
