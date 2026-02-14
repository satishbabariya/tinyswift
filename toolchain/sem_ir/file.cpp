// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "toolchain/sem_ir/file.h"

#include <optional>
#include <string>
#include <utility>

#include "common/check.h"
#include "llvm/ADT/SmallVector.h"
#include "toolchain/base/shared_value_stores.h"
#include "toolchain/base/yaml.h"
#include "toolchain/sem_ir/ids.h"
#include "toolchain/sem_ir/inst.h"
#include "toolchain/sem_ir/inst_kind.h"
#include "toolchain/sem_ir/typed_insts.h"

// TODO: Implement your language's semantic IR file here.
// See TinySwift compiler for reference implementation patterns.

namespace TinySwift::SemIR {

File::File(const Parse::Tree* parse_tree, CheckIRId check_ir_id,
           SharedValueStores& value_stores, std::string filename)
    : parse_tree_(parse_tree),
      check_ir_id_(check_ir_id),
      value_stores_(&value_stores),
      filename_(std::move(filename)),
      entity_names_(check_ir_id),
      functions_(check_ir_id),
      // The `+1` prevents adding a tag to the global `Namespace::PackageInstId`
      // instruction. It's not a "singleton" instruction, but it's a unique
      // instruction id that comes right after the singletons.
      insts_(this, SingletonInstKinds.size() + 1),
      constant_values_(ConstantId::NotConstant, &insts_),
      inst_blocks_(allocator_, check_ir_id),
      constants_(this) {
  // `type` and the error type are both complete & concrete types.
  types_.SetComplete(
      TypeType::TypeId,
      {.value_repr = {.kind = ValueRepr::Copy, .type_id = TypeType::TypeId}});
  types_.SetComplete(
      ErrorInst::TypeId,
      {.value_repr = {.kind = ValueRepr::Copy, .type_id = ErrorInst::TypeId}});

  insts_.Reserve(SingletonInstKinds.size());
  for (auto kind : SingletonInstKinds) {
    auto inst_id =
        insts_.AddInNoBlock(LocIdAndInst::NoLoc(Inst::MakeSingleton(kind)));
    constant_values_.Set(inst_id, ConstantId::ForConcreteConstant(inst_id));
  }
}

File::~File() = default;

auto File::Verify() const -> ErrorOr<Success> {
  // Invariants don't necessarily hold for invalid IR.
  if (has_errors_) {
    return Success();
  }

  // Check that every code block has a terminator sequence that appears at the
  // end of the block.
  for (const Function& function : functions_.values()) {
    for (InstBlockId block_id : function.body_block_ids) {
      TerminatorKind prior_kind = TerminatorKind::NotTerminator;
      for (InstId inst_id : inst_blocks().Get(block_id)) {
        TerminatorKind inst_kind =
            insts().Get(inst_id).kind().terminator_kind();
        if (prior_kind == TerminatorKind::Terminator) {
          return Error(llvm::formatv("Inst {0} in block {1} follows terminator",
                                     inst_id, block_id));
        }
        if (prior_kind > inst_kind) {
          return Error(
              llvm::formatv("Non-terminator inst {0} in block {1} follows "
                            "terminator sequence",
                            inst_id, block_id));
        }
        prior_kind = inst_kind;
      }
      if (prior_kind != TerminatorKind::Terminator) {
        return Error(llvm::formatv("No terminator in block {0}", block_id));
      }
    }
  }

  // TODO: Check that an instruction only references other instructions that are
  // either global or that dominate it.
  return Success();
}

auto File::OutputYaml(bool include_singletons) const -> Yaml::OutputMapping {
  return Yaml::OutputMapping([this,
                              include_singletons](Yaml::OutputMapping::Map map) {
    map.Add("filename", filename_);
    map.Add("sem_ir", Yaml::OutputMapping([&](Yaml::OutputMapping::Map map) {
              map.Add("name_scopes", name_scopes_.OutputYaml());
              map.Add("entity_names", entity_names_.OutputYaml());
              map.Add("functions", functions_.OutputYaml());
              map.Add("types", types_.OutputYaml());
              map.Add("insts",
                      Yaml::OutputMapping([&](Yaml::OutputMapping::Map map) {
                        for (auto [id, inst] : insts_.enumerate()) {
                          if (!include_singletons && IsSingletonInstId(id)) {
                            continue;
                          }
                          map.Add(PrintToString(id), Yaml::OutputScalar(inst));
                        }
                      }));
              map.Add("constant_values",
                      constant_values_.OutputYaml(include_singletons));
              map.Add("inst_blocks", inst_blocks_.OutputYaml());
              map.Add("value_stores", value_stores_->OutputYaml());
            }));
  });
}

auto File::CollectMemUsage(MemUsage& mem_usage, llvm::StringRef label) const
    -> void {
  mem_usage.Collect(MemUsage::ConcatLabel(label, "allocator_"), allocator_);
  mem_usage.Collect(MemUsage::ConcatLabel(label, "entity_names_"),
                    entity_names_);
  mem_usage.Collect(MemUsage::ConcatLabel(label, "functions_"), functions_);
  mem_usage.Collect(MemUsage::ConcatLabel(label, "insts_"), insts_);
  mem_usage.Collect(MemUsage::ConcatLabel(label, "name_scopes_"), name_scopes_);
  mem_usage.Collect(MemUsage::ConcatLabel(label, "constant_values_"),
                    constant_values_);
  mem_usage.Collect(MemUsage::ConcatLabel(label, "inst_blocks_"), inst_blocks_);
  mem_usage.Collect(MemUsage::ConcatLabel(label, "constants_"), constants_);
  mem_usage.Collect(MemUsage::ConcatLabel(label, "types_"), types_);
}

}  // namespace TinySwift::SemIR
