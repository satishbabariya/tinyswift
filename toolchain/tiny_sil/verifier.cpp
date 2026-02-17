// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "toolchain/tiny_sil/verifier.h"

#include "llvm/ADT/DenseSet.h"
#include "llvm/Support/raw_ostream.h"

namespace TinySwift::TinySIL {

namespace {

class SILVerifier {
 public:
  explicit SILVerifier(const SILModule& module, llvm::raw_ostream* error_stream)
      : module_(module),
        err_(error_stream ? *error_stream : llvm::nulls()),
        has_error_stream_(error_stream != nullptr) {}

  auto verify() -> bool {
    for (const auto& fn : module_.functions) {
      verifyFunction(*fn);
    }
    return !has_errors_;
  }

 private:
  auto emitError(llvm::StringRef msg) -> void {
    has_errors_ = true;
    if (has_error_stream_) {
      err_ << "SIL verification error: " << msg << "\n";
    }
  }

  auto emitError(const SILFunction& fn, llvm::StringRef msg) -> void {
    has_errors_ = true;
    if (has_error_stream_) {
      err_ << "SIL verification error in @" << fn.name << ": " << msg << "\n";
    }
  }

  auto emitError(const SILFunction& fn, const SILBasicBlock& bb,
                 llvm::StringRef msg) -> void {
    has_errors_ = true;
    if (has_error_stream_) {
      err_ << "SIL verification error in @" << fn.name << " bb" << bb.id
           << ": " << msg << "\n";
    }
  }

  auto verifyFunction(const SILFunction& fn) -> void {
    if (fn.name.empty()) {
      emitError("function has empty name");
    }

    // Declaration functions should have no blocks.
    if (fn.is_declaration) {
      if (!fn.blocks.empty()) {
        emitError(fn, "declaration function should have no blocks");
      }
      return;
    }

    // Functions with bodies must have at least one block.
    if (fn.blocks.empty()) {
      emitError(fn, "function with body has no basic blocks");
      return;
    }

    // Verify basic block IDs are sequential.
    for (int32_t i = 0; i < static_cast<int32_t>(fn.blocks.size()); ++i) {
      if (fn.blocks[i]->id != i) {
        emitError(fn, "basic block ID mismatch: expected " +
                          std::to_string(i) + " but got " +
                          std::to_string(fn.blocks[i]->id));
      }
    }

    // Collect defined value IDs.
    llvm::DenseSet<int32_t> defined_values;

    // Block arguments define values.
    for (const auto& bb : fn.blocks) {
      for (const auto& arg : bb->args) {
        if (arg.is_valid()) {
          if (!defined_values.insert(arg.id).second) {
            emitError(fn, *bb,
                      "duplicate value ID %" + std::to_string(arg.id));
          }
        }
      }
    }

    // Verify each block.
    for (const auto& bb : fn.blocks) {
      verifyBasicBlock(fn, *bb, defined_values);
    }
  }

  auto verifyBasicBlock(const SILFunction& fn, const SILBasicBlock& bb,
                        llvm::DenseSet<int32_t>& defined_values) -> void {
    if (bb.insts.empty()) {
      emitError(fn, bb, "basic block has no instructions");
      return;
    }

    // Every instruction result that is valid defines a value.
    for (const auto& inst : bb.insts) {
      if (inst->result.is_valid()) {
        if (!defined_values.insert(inst->result.id).second) {
          emitError(fn, bb,
                    "duplicate value ID %" + std::to_string(inst->result.id));
        }
      }
    }

    // Verify the block ends with a terminator.
    auto* last = bb.insts.back().get();
    if (!isTerminator(last->kind)) {
      emitError(fn, bb, "basic block does not end with a terminator");
    }

    // Verify no terminator appears before the last instruction.
    for (size_t i = 0; i + 1 < bb.insts.size(); ++i) {
      if (isTerminator(bb.insts[i]->kind)) {
        emitError(fn, bb, "terminator found before end of basic block");
      }
    }

    // Verify each instruction.
    for (const auto& inst : bb.insts) {
      verifyInstruction(fn, bb, *inst);
    }
  }

  auto verifyInstruction(const SILFunction& fn, const SILBasicBlock& bb,
                         const SILInstruction& inst) -> void {
    switch (inst.kind) {
      case SILInstKind::Branch:
        verifyBranchTarget(fn, bb, inst.target_block);
        break;

      case SILInstKind::CondBranch:
        if (!inst.operands[0].is_valid()) {
          emitError(fn, bb, "cond_br missing condition operand");
        }
        verifyBranchTarget(fn, bb, inst.true_block);
        verifyBranchTarget(fn, bb, inst.false_block);
        break;

      case SILInstKind::ReturnInst:
        if (!inst.operands[0].is_valid()) {
          emitError(fn, bb, "return missing operand");
        }
        break;

      case SILInstKind::Apply:
        if (!inst.operands[0].is_valid()) {
          emitError(fn, bb, "apply missing callee operand");
        }
        break;

      case SILInstKind::Store:
        if (!inst.operands[0].is_valid() || !inst.operands[1].is_valid()) {
          emitError(fn, bb, "store missing source or destination operand");
        }
        break;

      case SILInstKind::Load:
        if (!inst.operands[0].is_valid()) {
          emitError(fn, bb, "load missing address operand");
        }
        break;

      case SILInstKind::AllocStack:
        if (!inst.result.is_valid()) {
          emitError(fn, bb, "alloc_stack must produce a result");
        }
        break;

      case SILInstKind::IntegerLiteral:
      case SILInstKind::FloatLiteral:
      case SILInstKind::StringLiteral:
        if (!inst.result.is_valid()) {
          emitError(fn, bb, "literal must produce a result");
        }
        break;

      case SILInstKind::FunctionRef:
        if (inst.function_name.empty()) {
          emitError(fn, bb, "function_ref has empty function name");
        }
        if (!inst.result.is_valid()) {
          emitError(fn, bb, "function_ref must produce a result");
        }
        break;

      case SILInstKind::BuiltinInst:
        if (inst.builtin_name.empty()) {
          emitError(fn, bb, "builtin has empty name");
        }
        break;

      default:
        // No additional verification for other instructions.
        break;
    }
  }

  auto verifyBranchTarget(const SILFunction& fn, const SILBasicBlock& bb,
                          int32_t target) -> void {
    if (target < 0 ||
        target >= static_cast<int32_t>(fn.blocks.size())) {
      emitError(fn, bb,
                "branch target bb" + std::to_string(target) +
                    " is out of range");
    }
  }

  static auto isTerminator(SILInstKind kind) -> bool {
    switch (kind) {
      case SILInstKind::Branch:
      case SILInstKind::CondBranch:
      case SILInstKind::SwitchEnum:
      case SILInstKind::ReturnInst:
      case SILInstKind::Unreachable:
        return true;
      default:
        return false;
    }
  }

  const SILModule& module_;
  llvm::raw_ostream& err_;
  bool has_error_stream_;
  bool has_errors_ = false;
};

}  // namespace

auto VerifySILModule(const SILModule& module,
                     llvm::raw_ostream* error_stream) -> bool {
  SILVerifier verifier(module, error_stream);
  return verifier.verify();
}

}  // namespace TinySwift::TinySIL
