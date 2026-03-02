// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "toolchain/driver/compile_subcommand.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"
#include "common/pretty_stack_trace_function.h"
#include "common/vlog.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/ScopeExit.h"
#include "llvm/MC/TargetRegistry.h"
#include "toolchain/codegen/optimize.h"
#include "toolchain/codegen/target_machine.h"
#include "toolchain/base/clang_invocation.h"
#include "toolchain/base/timings.h"
#include "toolchain/check/check.h"
#include "toolchain/codegen/codegen.h"
#include "toolchain/linker/link.h"
#include "toolchain/diagnostics/emitter.h"
#include "toolchain/diagnostics/sorting_consumer.h"
#include "toolchain/lex/lex.h"
#include "toolchain/lower/lower.h"
#include "toolchain/parse/parse.h"
#include "toolchain/tiny_sil/module.h"
#include "toolchain/tiny_sil/printer.h"
#include "toolchain/tiny_sil/verifier.h"
#include "toolchain/tiny_sil_gen/sil_gen.h"
#include "toolchain/tiny_sil_optimizer/pass_manager.h"
#include "toolchain/parse/tree_and_subtrees.h"
#include "toolchain/sem_ir/ids.h"
#include "toolchain/source/source_buffer.h"

namespace TinySwift {

auto CompileOptions::Build(CommandLine::CommandBuilder& b) -> void {
  b.AddStringPositionalArg(
      {
          .name = "FILE",
          .help = R"""(
The input source file to compile.
)""",
      },
      [&](auto& arg_b) {
        arg_b.Required(true);
        arg_b.Append(&input_filenames);
      });

  b.AddOneOfOption(
      {
          .name = "phase",
          .help = R"""(
Selects the compilation phase to run. These phases are always run in sequence,
so every phase before the one selected will also be run. The default is to
compile to machine code.
)""",
      },
      [&](auto& arg_b) {
        arg_b.SetOneOf(
            {
                arg_b.OneOfValue("lex", Phase::Lex),
                arg_b.OneOfValue("parse", Phase::Parse),
                arg_b.OneOfValue("check", Phase::Check),
                arg_b.OneOfValue("sil", Phase::Sil),
                arg_b.OneOfValue("lower", Phase::Lower),
                arg_b.OneOfValue("optimize", Phase::Optimize),
                arg_b.OneOfValue("codegen", Phase::CodeGen).Default(true),
            },
            &phase);
      });

  b.AddStringOption(
      {
          .name = "output",
          .value_name = "FILE",
          .help = R"""(
The output filename for codegen.
)""",
      },
      [&](auto& arg_b) { arg_b.Set(&output_filename); });

  b.AddOneOfOption(
      {
          .name = "optimize",
          .help = R"""(
Selects the amount of optimization to perform.
)""",
      },
      [&](auto& arg_b) {
        arg_b.SetOneOf(
            {
                arg_b.OneOfValue("none", Lower::OptimizationLevel::None),
                arg_b.OneOfValue("debug", Lower::OptimizationLevel::Debug),
                arg_b.OneOfValue("speed", Lower::OptimizationLevel::Speed),
                arg_b.OneOfValue("size", Lower::OptimizationLevel::Size),
            },
            &opt_level);
      });

  codegen_options.Build(b);

  b.AddFlag(
      {
          .name = "asm-output",
          .help = "Write textual assembly rather than a binary object file.",
      },
      [&](auto& arg_b) { arg_b.Set(&asm_output); });

  b.AddFlag(
      {
          .name = "force-obj-output",
          .help = "Force binary object output, even with `--output=-`.",
      },
      [&](auto& arg_b) { arg_b.Set(&force_obj_output); });

  b.AddFlag(
      {
          .name = "stream-errors",
          .help = "Stream error messages to stderr as generated.",
      },
      [&](auto& arg_b) { arg_b.Set(&stream_errors); });

  b.AddFlag(
      {
          .name = "dump-shared-values",
          .help = "Dumps shared values.",
      },
      [&](auto& arg_b) { arg_b.Set(&dump_shared_values); });
  b.AddFlag(
      {
          .name = "dump-tokens",
          .help = "Dump the tokens to stdout when lexed.",
      },
      [&](auto& arg_b) { arg_b.Set(&dump_tokens); });
  b.AddFlag(
      {
          .name = "omit-file-boundary-tokens",
          .help = "For `--dump-tokens`, omit file start and end tokens.",
      },
      [&](auto& arg_b) { arg_b.Set(&omit_file_boundary_tokens); });
  b.AddFlag(
      {
          .name = "dump-parse-tree",
          .help = "Dump the parse tree to stdout when parsed.",
      },
      [&](auto& arg_b) { arg_b.Set(&dump_parse_tree); });
  b.AddFlag(
      {
          .name = "preorder-parse-tree",
          .help = "When dumping the parse tree, use preorder instead.",
      },
      [&](auto& arg_b) { arg_b.Set(&preorder_parse_tree); });
  b.AddFlag(
      {
          .name = "dump-raw-sem-ir",
          .help = "Dump the raw JSON structure of SemIR to stdout.",
      },
      [&](auto& arg_b) { arg_b.Set(&dump_raw_sem_ir); });
  b.AddFlag(
      {
          .name = "dump-sem-ir",
          .help = "Dump the full SemIR to stdout when built.",
      },
      [&](auto& arg_b) { arg_b.Set(&dump_sem_ir); });
  b.AddOneOfOption(
      {
          .name = "dump-sem-ir-ranges",
          .help = "Selects handling of SemIR range markers when dumping.",
      },
      [&](auto& arg_b) {
        using DumpSemIRRanges = Check::CheckParseTreesOptions::DumpSemIRRanges;
        arg_b.SetOneOf(
            {
                arg_b.OneOfValue("if-present", DumpSemIRRanges::IfPresent)
                    .Default(true),
                arg_b.OneOfValue("only", DumpSemIRRanges::Only),
                arg_b.OneOfValue("ignore", DumpSemIRRanges::Ignore),
            },
            &dump_sem_ir_ranges);
      });
  b.AddFlag(
      {
          .name = "builtin-sem-ir",
          .help = "Include the SemIR for builtins when dumping it.",
      },
      [&](auto& arg_b) { arg_b.Set(&builtin_sem_ir); });
  b.AddFlag(
      {
          .name = "dump-sil",
          .help = "Dump the TinySIL to stdout after SILGen.",
      },
      [&](auto& arg_b) { arg_b.Set(&dump_sil); });
  b.AddFlag(
      {
          .name = "dump-llvm-ir",
          .help = "Dump the LLVM IR to stdout after lowering.",
      },
      [&](auto& arg_b) { arg_b.Set(&dump_llvm_ir); });
  b.AddFlag(
      {
          .name = "dump-asm",
          .help = "Dump the generated assembly to stdout after codegen.",
      },
      [&](auto& arg_b) { arg_b.Set(&dump_asm); });
  b.AddFlag(
      {
          .name = "dump-mem-usage",
          .help = "Dumps the amount of memory used.",
      },
      [&](auto& arg_b) { arg_b.Set(&dump_mem_usage); });
  b.AddFlag(
      {
          .name = "dump-timings",
          .help = "Dumps the duration of each phase.",
      },
      [&](auto& arg_b) { arg_b.Set(&dump_timings); });
  b.AddFlag(
      {
          .name = "prelude-import",
          .help = "Whether to use the implicit prelude import (default: true).",
      },
      [&](auto& arg_b) {
        arg_b.Default(true);
        arg_b.Set(&prelude_import);
      });
  b.AddStringOption(
      {
          .name = "exclude-dump-file-prefix",
          .value_name = "PREFIX",
          .help = "Excludes files with the given prefix from dumps.",
      },
      [&](auto& arg_b) { arg_b.Append(&exclude_dump_file_prefixes); });
  b.AddStringOption(
      {
          .name = "define",
          .value_name = "FLAG",
          .help = "Define a conditional compilation flag (can be repeated).",
      },
      [&](auto& arg_b) { arg_b.Append(&defines); });
  b.AddFlag(
      {
          .name = "debug-info",
          .help = "Whether to emit DWARF debug information.",
      },
      [&](auto& arg_b) {
        arg_b.Default(true);
        arg_b.Set(&include_debug_info);
      });
  b.AddFlag(
      {
          .name = "verify-llvm-ir",
          .help = "Whether to run the LLVM verifier on modules.",
      },
      [&](auto& arg_b) {
        arg_b.Default(true);
        arg_b.Set(&run_llvm_verifier);
      });

  // M115: Linking flags.
  b.AddFlag(
      {
          .name = "emit-object",
          .help = "Emit an object file (.o) without linking.",
      },
      [&](auto& arg_b) { arg_b.Set(&emit_object); });
  b.AddFlag(
      {
          .name = "release",
          .help = "Build in release mode (O3, Thin LTO, strip).",
      },
      [&](auto& arg_b) { arg_b.Set(&release); });
  b.AddFlag(
      {
          .name = "lto-thin",
          .help = "Enable Thin LTO.",
      },
      [&](auto& arg_b) { arg_b.Set(&lto_thin); });
  b.AddFlag(
      {
          .name = "emit-bitcode",
          .help = "Emit LLVM bitcode (.bc) instead of object code.",
      },
      [&](auto& arg_b) { arg_b.Set(&emit_bitcode); });
  b.AddStringOption(
      {
          .name = "link-lib",
          .value_name = "NAME",
          .help = "Link against library NAME (can be repeated).",
      },
      [&](auto& arg_b) { arg_b.Append(&link_libs); });
}

static constexpr CommandLine::CommandInfo SubcommandInfo = {
    .name = "compile",
    .help = R"""(
Compile source code.

This subcommand runs the compiler over input source code, checking it for
errors and producing the requested output.

Error messages are written to the standard error stream.

Different phases of the compiler can be selected to run, and intermediate state
can be written to standard output as these phases progress.
)""",
};

CompileSubcommand::CompileSubcommand() : DriverSubcommand(SubcommandInfo) {}

static auto PhaseToString(CompileOptions::Phase phase) -> std::string {
  switch (phase) {
    case CompileOptions::Phase::Lex:
      return "lex";
    case CompileOptions::Phase::Parse:
      return "parse";
    case CompileOptions::Phase::Check:
      return "check";
    case CompileOptions::Phase::Sil:
      return "sil";
    case CompileOptions::Phase::Lower:
      return "lower";
    case CompileOptions::Phase::Optimize:
      return "optimize";
    case CompileOptions::Phase::CodeGen:
      return "codegen";
  }
}

auto CompileSubcommand::ValidateOptions(
    Diagnostics::NoLocEmitter& emitter) const -> bool {
  TINYSWIFT_DIAGNOSTIC(
      CompilePhaseFlagConflict, Error,
      "requested dumping {0} but compile phase is limited to `{1}`",
      std::string, std::string);
  using Phase = CompileOptions::Phase;
  switch (options_.phase) {
    case Phase::Lex:
      if (options_.dump_parse_tree) {
        emitter.Emit(CompilePhaseFlagConflict, "parse tree",
                     PhaseToString(options_.phase));
        return false;
      }
      [[fallthrough]];
    case Phase::Parse:
      if (options_.dump_sem_ir) {
        emitter.Emit(CompilePhaseFlagConflict, "SemIR",
                     PhaseToString(options_.phase));
        return false;
      }
      [[fallthrough]];
    case Phase::Check:
      if (options_.dump_sil) {
        emitter.Emit(CompilePhaseFlagConflict, "SIL",
                     PhaseToString(options_.phase));
        return false;
      }
      [[fallthrough]];
    case Phase::Sil:
      if (options_.dump_llvm_ir) {
        emitter.Emit(CompilePhaseFlagConflict, "LLVM IR",
                     PhaseToString(options_.phase));
        return false;
      }
      [[fallthrough]];
    case Phase::Lower:
    case Phase::Optimize:
    case Phase::CodeGen:
      break;
  }
  return true;
}

namespace {

// Ties together information for a file being compiled.
class CompilationUnit {
 public:
  explicit CompilationUnit(SemIR::CheckIRId check_ir_id, int total_ir_count,
                           DriverEnv* driver_env, const CompileOptions* options,
                           Diagnostics::Consumer* consumer,
                           llvm::StringRef input_filename,
                           const llvm::Target* target);

  auto RunLex() -> void;
  auto RunParse() -> void;
  auto GetCheckUnit() -> Check::Unit;
  auto PostCheck() -> void;
  auto RunSilGen() -> void;
  auto PostSilGen() -> void;
  auto RunLower() -> void;
  auto RunOptimize() -> void;
  auto PostLower() -> void;
  auto RunCodeGen() -> void;
  auto PostCompile() -> void;

  auto FlushForStackTrace() -> void { consumer_->Flush(); }
  auto input_filename() -> llvm::StringRef { return input_filename_; }
  auto success() -> bool { return success_; }
  auto has_source() -> bool { return source_.has_value(); }
  auto get_trees_and_subtrees() -> Parse::GetTreeAndSubtreesFn {
    return *tree_and_subtrees_getter_;
  }

 private:
  auto RunCodeGenHelper() -> bool;
  auto GetParseTreeAndSubtrees() -> const Parse::TreeAndSubtrees&;
  auto LogCall(llvm::StringLiteral logging_label,
               llvm::StringLiteral timing_label,
               llvm::function_ref<auto()->void> fn) -> void;
  auto IncludeInDumps() -> bool;
  auto MakeTargetMachine() -> void;

  SemIR::CheckIRId check_ir_id_;
  int total_ir_count_;

  DriverEnv* driver_env_;
  const CompileOptions* options_;
  const llvm::Target* target_;

  SharedValueStores value_stores_;
  std::string input_filename_;
  llvm::raw_pwrite_stream* vlog_stream_;

  std::optional<Diagnostics::SortingConsumer> sorting_consumer_;
  Diagnostics::Consumer* consumer_;

  bool success_ = true;

  std::optional<MemUsage> mem_usage_;
  std::optional<Timings> timings_;

  std::optional<SourceBuffer> source_;
  std::optional<Lex::TokenizedBuffer> tokens_;
  std::optional<Parse::Tree> parse_tree_;
  std::optional<Parse::TreeAndSubtrees> parse_tree_and_subtrees_;
  std::optional<std::function<auto()->const Parse::TreeAndSubtrees&>>
      tree_and_subtrees_getter_;
  std::unique_ptr<llvm::LLVMContext> llvm_context_;
  std::optional<SemIR::File> sem_ir_;
  std::unique_ptr<TinySIL::SILModule> sil_module_;
  std::unique_ptr<llvm::Module> module_;
  std::unique_ptr<llvm::TargetMachine> target_machine_;
};

}  // namespace

CompilationUnit::CompilationUnit(SemIR::CheckIRId check_ir_id,
                                 int total_ir_count, DriverEnv* driver_env,
                                 const CompileOptions* options,
                                 Diagnostics::Consumer* consumer,
                                 llvm::StringRef input_filename,
                                 const llvm::Target* target)
    : check_ir_id_(check_ir_id),
      total_ir_count_(total_ir_count),
      driver_env_(driver_env),
      options_(options),
      target_(target),
      input_filename_(input_filename),
      vlog_stream_(driver_env_->vlog_stream) {
  if (vlog_stream_ != nullptr || options_->stream_errors) {
    consumer_ = consumer;
  } else {
    sorting_consumer_ = Diagnostics::SortingConsumer(*consumer);
    consumer_ = &*sorting_consumer_;
  }

  if (options_->dump_mem_usage) {
    mem_usage_ = MemUsage();
  }
  if (options_->dump_timings) {
    timings_ = Timings();
  }
}

auto CompilationUnit::IncludeInDumps() -> bool {
  return llvm::none_of(options_->exclude_dump_file_prefixes,
                       [&](auto prefix) {
                         return input_filename_.starts_with(prefix.str());
                       });
}

auto CompilationUnit::RunLex() -> void {
  LogCall("SourceBuffer::MakeFromFileOrStdin", "source", [&] {
    source_ = SourceBuffer::MakeFromFileOrStdin(*driver_env_->fs,
                                                input_filename_, *consumer_);
  });

  if (!source_) {
    success_ = false;
    return;
  }

  if (mem_usage_) {
    mem_usage_->Add("source_", source_->text().size(), source_->text().size());
  }

  TINYSWIFT_VLOG("*** SourceBuffer ***\n```\n{0}\n```\n", source_->text());

  LogCall("Lex::Lex", "lex", [&] {
    Lex::LexOptions options;
    options.consumer = consumer_;
    options.vlog_stream = vlog_stream_;
    if (options_->dump_tokens && IncludeInDumps()) {
      options.dump_stream = driver_env_->output_stream;
      options.omit_file_boundary_tokens = options_->omit_file_boundary_tokens;
    }
    tokens_ = Lex::Lex(value_stores_, *source_, options);
  });
  if (mem_usage_) {
    mem_usage_->Collect("tokens_", *tokens_);
  }
  if (tokens_->has_errors()) {
    success_ = false;
  }
}

auto CompilationUnit::RunParse() -> void {
  LogCall("Parse::Parse", "parse", [&] {
    Parse::ParseOptions options;
    options.consumer = consumer_;
    options.vlog_stream = vlog_stream_;
    if (options_->dump_parse_tree && IncludeInDumps()) {
      options.dump_stream = driver_env_->output_stream;
      options.dump_preorder_parse_tree = options_->preorder_parse_tree;
    }
    parse_tree_ = Parse::Parse(*tokens_, options);
  });
  if (mem_usage_) {
    mem_usage_->Collect("parse_tree_", *parse_tree_);
  }
  if (parse_tree_->has_errors()) {
    success_ = false;
  }
}

auto CompilationUnit::GetCheckUnit() -> Check::Unit {
  TINYSWIFT_CHECK(parse_tree_, "Must call RunParse first");
  TINYSWIFT_CHECK(!sem_ir_, "Called GetCheckUnit twice");

  tree_and_subtrees_getter_ = [this]() -> const Parse::TreeAndSubtrees& {
    return this->GetParseTreeAndSubtrees();
  };
  sem_ir_.emplace(&*parse_tree_, check_ir_id_, value_stores_,
                  input_filename_);
  if (!llvm_context_) {
    llvm_context_ = std::make_unique<llvm::LLVMContext>();
  }
  return {.consumer = consumer_,
          .value_stores = &value_stores_,
          .timings = timings_ ? &*timings_ : nullptr,
          .sem_ir = &*sem_ir_,
          .llvm_context = llvm_context_.get(),
          .total_ir_count = total_ir_count_};
}

auto CompilationUnit::PostCheck() -> void {
  TINYSWIFT_CHECK(sem_ir_, "Must call GetCheckUnit first");
  consumer_->Flush();

  if (mem_usage_) {
    mem_usage_->Collect("sem_ir_", *sem_ir_);
  }

  if (sem_ir_->has_errors()) {
    success_ = false;
  }
}

auto CompilationUnit::RunSilGen() -> void {
  LogCall("TinySILGen::GenerateSIL", "silgen", [&] {
    TINYSWIFT_CHECK(sem_ir_, "Must call GetCheckUnit/PostCheck first");
    sil_module_ = TinySILGen::GenerateSIL(*sem_ir_);
  });
}

auto CompilationUnit::PostSilGen() -> void {
  TINYSWIFT_CHECK(sil_module_, "Must call RunSilGen first");

  // Verify the SIL module structure.
  if (!TinySIL::VerifySILModule(*sil_module_, driver_env_->error_stream)) {
    success_ = false;
  }

  // Run mandatory diagnostic passes.
  if (!TinySILOptimizer::RunMandatoryPasses(*sil_module_,
                                             driver_env_->error_stream)) {
    success_ = false;
  }

  // Run performance optimization passes.
  TinySILOptimizer::RunPerformancePasses(*sil_module_);

  // Dump SIL if requested.
  if (options_->dump_sil && IncludeInDumps()) {
    TinySIL::PrintSILModule(*driver_env_->output_stream, *sil_module_);
  }
}

auto CompilationUnit::RunLower() -> void {
  LogCall("Lower::LowerToLLVM", "lower", [&] {
    if (!llvm_context_) {
      llvm_context_ = std::make_unique<llvm::LLVMContext>();
    }
    Lower::LowerToLLVMOptions options;
    options.llvm_verifier_stream =
        options_->run_llvm_verifier ? driver_env_->error_stream : nullptr;
    options.want_debug_info = options_->include_debug_info;
    options.vlog_stream = vlog_stream_;
    options.opt_level = options_->opt_level;

    // M119: Pass tokenized buffer for DWARF debug info source locations.
    const Lex::TokenizedBuffer* tokens_ptr =
        tokens_.has_value() ? &*tokens_ : nullptr;

    if (sil_module_) {
      // Use the SIL-based lowering path.
      module_ = Lower::LowerSILToLLVM(*llvm_context_, input_filename_,
                                       *sil_module_, *sem_ir_, options,
                                       tokens_ptr);
    } else {
      // Fallback: direct SemIR → LLVM path.
      module_ = Lower::LowerToLLVM(*llvm_context_, input_filename_,
                                   *sem_ir_, options, tokens_ptr);
    }
  });
}

auto CompilationUnit::MakeTargetMachine() -> void {
  TINYSWIFT_CHECK(module_, "Must call RunLower first");
  TINYSWIFT_CHECK(!target_machine_, "Should not call this multiple times");

  target_machine_ = TinySwift::MakeTargetMachine(
      target_, options_->codegen_options.target, *module_);
}

auto CompilationUnit::RunOptimize() -> void {
  TINYSWIFT_CHECK(module_, "Must call RunLower first");

  MakeTargetMachine();

  LogCall("ModulePassManager::run", "optimize", [&] {
    RunLLVMOptimizePipeline(*module_, *target_machine_,
                            options_->opt_level, vlog_stream_);
  });
}

auto CompilationUnit::PostLower() -> void {
  TINYSWIFT_CHECK(module_, "Must call RunLower first");
  if (options_->dump_llvm_ir && IncludeInDumps()) {
    module_->print(*driver_env_->output_stream, /*AAW=*/nullptr,
                   /*ShouldPreserveUseListOrder=*/true);
  }
}

auto CompilationUnit::RunCodeGen() -> void {
  TINYSWIFT_CHECK(module_, "Must call RunLower first");
  LogCall("CodeGen", "codegen", [&] { success_ = RunCodeGenHelper(); });
}

auto CompilationUnit::PostCompile() -> void {
  if (options_->dump_shared_values && IncludeInDumps()) {
    Yaml::Print(*driver_env_->output_stream,
                value_stores_.OutputYaml(input_filename_));
  }
  if (mem_usage_) {
    mem_usage_->Collect("value_stores_", value_stores_);
    Yaml::Print(*driver_env_->output_stream,
                mem_usage_->OutputYaml(input_filename_));
  }
  if (timings_) {
    Yaml::Print(*driver_env_->output_stream,
                timings_->OutputYaml(input_filename_));
  }
  consumer_->Flush();
}

auto CompilationUnit::RunCodeGenHelper() -> bool {
  TINYSWIFT_CHECK(module_, "Must call RunLower first");
  TINYSWIFT_CHECK(target_machine_, "Must call MakeTargetMachine first");

  CodeGen codegen(module_.get(), target_machine_.get(), consumer_);
  if (vlog_stream_) {
    TINYSWIFT_VLOG("*** Assembly ***\n");
    codegen.EmitAssembly(*vlog_stream_);
  }

  if (options_->output_filename == "-") {
    if (options_->emit_bitcode) {
      codegen.EmitBitcode(*driver_env_->output_stream);
    } else if (options_->force_obj_output) {
      if (!codegen.EmitObject(*driver_env_->output_stream)) {
        return false;
      }
    } else {
      if (!codegen.EmitAssembly(*driver_env_->output_stream)) {
        return false;
      }
    }
  } else {
    llvm::SmallString<256> output_filename = options_->output_filename;
    if (output_filename.empty()) {
      if (!source_->is_regular_file()) {
        TINYSWIFT_DIAGNOSTIC(CompileInputNotRegularFile, Error,
                          "output file name must be specified for input `{0}` "
                          "that is not a regular file",
                          std::string);
        driver_env_->emitter.Emit(CompileInputNotRegularFile, input_filename_);
        return false;
      }
      output_filename = input_filename_;
      if (options_->emit_bitcode) {
        llvm::sys::path::replace_extension(output_filename, ".bc");
      } else if (options_->asm_output) {
        llvm::sys::path::replace_extension(output_filename, ".s");
      } else if (options_->emit_object) {
        llvm::sys::path::replace_extension(output_filename, ".o");
      } else {
        // Default: executable (no extension on Unix, remove .swift).
        llvm::sys::path::replace_extension(output_filename, "");
      }
    }
    TINYSWIFT_VLOG("Writing output to: {0}\n", output_filename);

    // M115: Emit bitcode directly if requested.
    if (options_->emit_bitcode) {
      std::error_code ec;
      llvm::raw_fd_ostream output_file(output_filename, ec,
                                       llvm::sys::fs::OF_None);
      if (ec) {
        TINYSWIFT_DIAGNOSTIC(CompileOutputFileOpenError, Error,
                          "could not open output file `{0}`: {1}", std::string,
                          std::string);
        driver_env_->emitter.Emit(CompileOutputFileOpenError,
                                  output_filename.str().str(), ec.message());
        return false;
      }
      return codegen.EmitBitcode(output_file);
    }

    // M115: If --emit-object or --asm-output, produce .o/.s without linking.
    if (options_->emit_object || options_->asm_output) {
      std::error_code ec;
      llvm::raw_fd_ostream output_file(output_filename, ec,
                                       llvm::sys::fs::OF_None);
      if (ec) {
        TINYSWIFT_DIAGNOSTIC(CompileOutputFileOpenError, Error,
                          "could not open output file `{0}`: {1}", std::string,
                          std::string);
        driver_env_->emitter.Emit(CompileOutputFileOpenError,
                                  output_filename.str().str(), ec.message());
        return false;
      }
      if (options_->asm_output) {
        return codegen.EmitAssembly(output_file);
      }
      return codegen.EmitObject(output_file);
    }

    // M115: Default — emit .o to a temp file, then link to executable.
    llvm::SmallString<256> temp_obj_path;
    {
      std::error_code ec =
          llvm::sys::fs::createTemporaryFile("tinyswift", "o", temp_obj_path);
      if (ec) {
        TINYSWIFT_DIAGNOSTIC(CompileTempFileError, Error,
                          "could not create temporary file: {0}", std::string);
        driver_env_->emitter.Emit(CompileTempFileError, ec.message());
        return false;
      }
    }
    // Clean up the temp file on exit.
    auto cleanup = llvm::scope_exit(
        [&] { llvm::sys::fs::remove(temp_obj_path); });

    {
      std::error_code ec;
      llvm::raw_fd_ostream temp_file(temp_obj_path, ec,
                                     llvm::sys::fs::OF_None);
      if (ec) {
        TINYSWIFT_DIAGNOSTIC(CompileOutputFileOpenError, Error,
                          "could not open output file `{0}`: {1}", std::string,
                          std::string);
        driver_env_->emitter.Emit(CompileOutputFileOpenError,
                                  temp_obj_path.str().str(), ec.message());
        return false;
      }
      if (!codegen.EmitObject(temp_file)) {
        return false;
      }
    }

    // Link the temp .o into an executable.
    LinkOptions link_opts;
    link_opts.output_path = output_filename;
    link_opts.object_files.push_back(temp_obj_path.str().str());
    for (const auto& lib : options_->link_libs) {
      link_opts.link_libs.push_back(lib.str());
    }
    link_opts.dead_strip = true;
    link_opts.strip = options_->release;
    link_opts.lto_thin = options_->release || options_->lto_thin;
    link_opts.target_triple = options_->codegen_options.target;

    return InvokeLinker(*driver_env_->installation, link_opts, *consumer_);
  }
  return true;
}

auto CompilationUnit::GetParseTreeAndSubtrees()
    -> const Parse::TreeAndSubtrees& {
  if (!parse_tree_and_subtrees_) {
    parse_tree_and_subtrees_ = Parse::TreeAndSubtrees(*tokens_, *parse_tree_);
    if (mem_usage_) {
      mem_usage_->Collect("parse_tree_and_subtrees_",
                          *parse_tree_and_subtrees_);
    }
  }
  return *parse_tree_and_subtrees_;
}

auto CompilationUnit::LogCall(llvm::StringLiteral logging_label,
                              llvm::StringLiteral timing_label,
                              llvm::function_ref<auto()->void> fn) -> void {
  PrettyStackTraceFunction trace_file([&](llvm::raw_ostream& out) {
    out << "Filename: " << input_filename_ << "\n";
  });
  TINYSWIFT_VLOG("*** {0}: {1} ***\n", logging_label, input_filename_);
  Timings::ScopedTiming timing(timings_ ? &*timings_ : nullptr, timing_label);
  fn();
  TINYSWIFT_VLOG("*** {0} done ***\n", logging_label);
}

// M88: Find the core/ directory containing prelude .swift files.
// Checks TINYSWIFT_CORE_DIR env var, then looks relative to the binary
// output directory (bazel-bin/toolchain/../../../core/).
static auto FindCoreDirectory() -> std::string {
  // 1. Environment variable override.
  if (const char* env = std::getenv("TINYSWIFT_CORE_DIR")) {
    return std::string(env);
  }
  // 2. Look for core/ relative to CWD (works in repo root).
  if (llvm::sys::fs::is_directory("core")) {
    return "core";
  }
  // 3. Try relative to the bazel workspace root patterns.
  for (const char* rel :
       {"../core", "../../core", "../../../core",
        "tinyswift/core"}) {
    if (llvm::sys::fs::is_directory(rel)) {
      return std::string(rel);
    }
  }
  return "";
}

auto CompileSubcommand::Run(DriverEnv& driver_env) -> DriverResult {
  if (!ValidateOptions(driver_env.emitter)) {
    return {.success = false};
  }

  // M115: Apply --release mode defaults.
  if (options_.release) {
    options_.opt_level = Lower::OptimizationLevel::Speed;
    options_.lto_thin = true;
  }

  // Validate the target.
  std::string target_error;
  const llvm::Target* target = llvm::TargetRegistry::lookupTarget(
      llvm::Triple(options_.codegen_options.target), target_error);
  if (!target) {
    TINYSWIFT_DIAGNOSTIC(CompileTargetInvalid, Error, "invalid target: {0}",
                      std::string);
    driver_env.emitter.Emit(CompileTargetInvalid, target_error);
    return {.success = false};
  }

  // Build a clang invocation for C++ interop support.
  std::shared_ptr<clang::CompilerInvocation> clang_invocation;
  {
    clang_invocation = BuildClangInvocation(
        driver_env.consumer, driver_env.fs, *driver_env.installation,
        options_.codegen_options.target);
    if (!clang_invocation) {
      return {.success = false};
    }
    clang_invocation->getCodeGenOpts().DisableLLVMPasses = true;
  }

  // M88: Collect prelude files if --prelude-import is enabled.
  llvm::SmallVector<std::string> prelude_files;
  if (options_.prelude_import) {
    auto core_dir = FindCoreDirectory();
    if (!core_dir.empty()) {
      std::error_code ec;
      for (llvm::sys::fs::directory_iterator it(core_dir, ec), end;
           it != end && !ec; it.increment(ec)) {
        auto path = it->path();
        if (llvm::StringRef(path).ends_with(".swift")) {
          prelude_files.push_back(path);
        }
      }
      std::sort(prelude_files.begin(), prelude_files.end());
    }
  }

  // Create CompilationUnits (prelude files first, then user files).
  // Module units will be inserted after lex+parse when we know what's imported.
  llvm::SmallVector<std::unique_ptr<CompilationUnit>> units;
  int prelude_count = static_cast<int>(prelude_files.size());
  int user_count = static_cast<int>(options_.input_filenames.size());
  int initial_total = prelude_count + user_count;
  for (int i = 0; i < prelude_count; ++i) {
    units.push_back(std::make_unique<CompilationUnit>(
        SemIR::CheckIRId(i), initial_total, &driver_env, &options_,
        &driver_env.consumer, prelude_files[i], target));
  }
  for (int i = 0; i < user_count; ++i) {
    units.push_back(std::make_unique<CompilationUnit>(
        SemIR::CheckIRId(prelude_count + i), initial_total, &driver_env,
        &options_, &driver_env.consumer, options_.input_filenames[i], target));
  }

  auto on_exit = llvm::scope_exit([&]() {
    for (auto& unit : units) {
      unit->PostCompile();
    }
    driver_env.consumer.Flush();
  });

  PrettyStackTraceFunction flush_on_crash([&](llvm::raw_ostream& out) {
    if (options_.stream_errors) {
      out << "Flushing diagnostics\n";
    } else {
      out << "Pending diagnostics:\n";
      driver_env.consumer.set_stream(&out);
    }
    for (auto& unit : units) {
      unit->FlushForStackTrace();
    }
    driver_env.consumer.Flush();
    driver_env.consumer.set_stream(driver_env.error_stream);
  });

  auto make_result = [&]() {
    DriverResult result = {.success = true};
    for (const auto& unit : units) {
      result.success &= unit->success();
      result.per_file_success.push_back(
          {unit->input_filename().str(), unit->success()});
    }
    return result;
  };

  // Lex.
  for (auto& unit : units) {
    unit->RunLex();
  }
  if (options_.phase == CompileOptions::Phase::Lex) {
    return make_result();
  }

  // Parse.
  for (auto& unit : units) {
    if (unit->has_source()) {
      unit->RunParse();
    }
  }
  if (options_.phase == CompileOptions::Phase::Parse) {
    return make_result();
  }

  // Check.
  llvm::SmallVector<Check::Unit> check_units;
  check_units.reserve(units.size());
  for (auto& unit : units) {
    if (unit->has_source()) {
      check_units.push_back(unit->GetCheckUnit());
    }
  }

  TINYSWIFT_VLOG_TO(driver_env.vlog_stream, "*** Check::CheckParseTrees ***\n");
  Check::CheckParseTreesOptions check_options;
  check_options.prelude_import = options_.prelude_import;
  check_options.vlog_stream = driver_env.vlog_stream;
  check_options.fuzzing = driver_env.fuzzing;
  check_options.defines = options_.defines;  // M107

  // Set up tree_and_subtrees_getters for checking.
  auto tree_and_subtrees_getters =
      Parse::GetTreeAndSubtreesStore::MakeWithExplicitSize(units.size(),
                                                            nullptr);
  for (const auto& [i, unit] : llvm::enumerate(units)) {
    if (unit->has_source()) {
      tree_and_subtrees_getters.Set(SemIR::CheckIRId(i),
                                    unit->get_trees_and_subtrees());
    }
  }

  // Set up include_in_dumps for checking.
  auto include_in_dumps =
      FixedSizeValueStore<SemIR::CheckIRId, bool>::MakeWithExplicitSize(
          units.size(), true);

  if (driver_env.vlog_stream || options_.dump_sem_ir || options_.dump_raw_sem_ir) {
    check_options.include_in_dumps = &include_in_dumps;
    if (options_.dump_sem_ir) {
      check_options.dump_stream = driver_env.output_stream;
    }
    if (driver_env.vlog_stream || options_.dump_sem_ir) {
      check_options.dump_sem_ir_ranges = options_.dump_sem_ir_ranges;
    }
    if (options_.dump_raw_sem_ir) {
      check_options.raw_dump_stream = driver_env.output_stream;
      check_options.dump_raw_sem_ir_builtins = options_.builtin_sem_ir;
    }
  }

  Check::CheckParseTrees(check_units, tree_and_subtrees_getters,
                         driver_env.fs, check_options, clang_invocation);
  TINYSWIFT_VLOG_TO(driver_env.vlog_stream,
                 "*** Check::CheckParseTrees done ***\n");
  for (auto& unit : units) {
    if (unit->has_source()) {
      unit->PostCheck();
    }
  }
  if (options_.phase == CompileOptions::Phase::Check) {
    return make_result();
  }

  // Errors block further progress.
  if (llvm::any_of(units, [&](const auto& unit) { return !unit->success(); })) {
    TINYSWIFT_VLOG_TO(driver_env.vlog_stream,
                   "*** Stopping before lowering due to errors ***\n");
    return make_result();
  }

  // M73: Multi-file compilation — all SemIR accumulated into primary unit
  // (units[0]). Only run SILGen/Lower/CodeGen on the primary unit.

  // SILGen (primary unit only).
  if (units[0]->has_source()) {
    units[0]->RunSilGen();
    units[0]->PostSilGen();
  }
  if (options_.phase == CompileOptions::Phase::Sil) {
    return make_result();
  }

  // Lower and optimize (primary unit only).
  if (units[0]->has_source()) {
    units[0]->RunLower();

    if (options_.phase != CompileOptions::Phase::Lower) {
      units[0]->RunOptimize();
    }

    units[0]->PostLower();
  }
  if (options_.phase == CompileOptions::Phase::Lower ||
      options_.phase == CompileOptions::Phase::Optimize) {
    return make_result();
  }
  TINYSWIFT_CHECK(options_.phase == CompileOptions::Phase::CodeGen,
               "CodeGen should be the last stage");

  // Codegen (primary unit only).
  if (units[0]->has_source()) {
    units[0]->RunCodeGen();
  }
  return make_result();
}

}  // namespace TinySwift
