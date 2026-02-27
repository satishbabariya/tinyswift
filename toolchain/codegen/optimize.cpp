// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "toolchain/codegen/optimize.h"

#include "common/vlog.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Passes/OptimizationLevel.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/StandardInstrumentations.h"

namespace TinySwift {

static auto GetLLVMOptimizationLevel(Lower::OptimizationLevel opt_level)
    -> llvm::OptimizationLevel {
  switch (opt_level) {
    case Lower::OptimizationLevel::None:
      return llvm::OptimizationLevel::O0;
    case Lower::OptimizationLevel::Debug:
      return llvm::OptimizationLevel::O1;
    case Lower::OptimizationLevel::Size:
      return llvm::OptimizationLevel::Oz;
    case Lower::OptimizationLevel::Speed:
      return llvm::OptimizationLevel::O3;
  }
}

auto RunLLVMOptimizePipeline(llvm::Module& module,
                             llvm::TargetMachine& target_machine,
                             Lower::OptimizationLevel opt_level,
                             llvm::raw_ostream* vlog_stream_) -> void {
  llvm::PipelineTuningOptions pto;
  bool opt_for_speed = opt_level == Lower::OptimizationLevel::Speed;
  bool opt_for_size_or_speed =
      opt_for_speed || opt_level == Lower::OptimizationLevel::Size;
  pto.LoopUnrolling = opt_for_size_or_speed;
  pto.LoopInterleaving = opt_for_size_or_speed;
  pto.LoopVectorization = opt_for_speed;
  pto.SLPVectorization = opt_for_size_or_speed;

  llvm::LoopAnalysisManager lam;
  llvm::FunctionAnalysisManager fam;
  llvm::CGSCCAnalysisManager cgam;
  llvm::ModuleAnalysisManager mam;

  llvm::PassInstrumentationCallbacks pic;

  llvm::StandardInstrumentations si(module.getContext(),
                                    /*DebugLogging=*/false);
  si.registerCallbacks(pic);

  llvm::PassBuilder builder(&target_machine, pto,
                            /*PGOOpt=*/std::nullopt, &pic);

  auto tlii = std::make_unique<llvm::TargetLibraryInfoImpl>(
      module.getTargetTriple());
  fam.registerPass([&] { return llvm::TargetLibraryAnalysis(*tlii); });

  builder.registerModuleAnalyses(mam);
  builder.registerCGSCCAnalyses(cgam);
  builder.registerFunctionAnalyses(fam);
  builder.registerLoopAnalyses(lam);
  builder.crossRegisterProxies(lam, fam, cgam, mam);

  llvm::ModulePassManager pass_manager =
      builder.buildPerModuleDefaultPipeline(GetLLVMOptimizationLevel(opt_level));

  if (vlog_stream_) {
    TINYSWIFT_VLOG("*** Running pass pipeline: ");
    pass_manager.printPipeline(
        *vlog_stream_, [&pic](llvm::StringRef class_name) {
          auto pass_name = pic.getPassNameForClassName(class_name);
          return pass_name.empty() ? class_name : pass_name;
        });
    TINYSWIFT_VLOG(" ***\n");
  }

  pass_manager.run(module, mam);

  if (vlog_stream_) {
    TINYSWIFT_VLOG("*** Optimized llvm::Module ***\n");
    module.print(*vlog_stream_, /*AAW=*/nullptr,
                 /*ShouldPreserveUseListOrder=*/false,
                 /*IsForDebug=*/true);
  }
}

}  // namespace TinySwift
