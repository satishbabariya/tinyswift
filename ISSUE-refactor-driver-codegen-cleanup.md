# Refactor: Move misplaced codegen/linking code out of driver/ and clean up dead includes

## Summary

The `toolchain/driver/` directory contains code that belongs in dedicated pipeline directories, and `toolchain/codegen/codegen.cpp` has dead includes leftover from a prior refactor. The driver should be a thin orchestration layer — CLI parsing and subcommand dispatch — not a home for codegen configuration, LLVM optimization pipelines, target machine setup, or linker invocation logic.

---

## 1. `driver/link.h` / `driver/link.cpp` → `toolchain/linker/`

**What:** `LinkOptions` struct, `InvokeLinker()`, `InvokeArchiver()`, `FindCC()` — a self-contained linking subsystem with platform-specific flags (dead stripping, LTO, strip, `-lm` on Linux).

**Why it's misplaced:** Linking is a post-codegen backend phase, not driver plumbing. It's consumed by three separate subcommands:
- `compile_subcommand.cpp:889`
- `build_subcommand.cpp:187,205`
- `test_subcommand.cpp:318`

**Proposed:** Move to `toolchain/linker/link.h` and `toolchain/linker/link.cpp` with its own BUILD target.

---

## 2. `driver/codegen_options.h` / `driver/codegen_options.cpp` → `toolchain/codegen/`

**What:** `CodegenOptions` struct holding the target triple and host triple detection.

**Why it's misplaced:** The target triple is codegen configuration. It's already a separate Bazel target (`//toolchain/driver:codegen_options`), which is a signal it doesn't belong in `driver/`. The `codegen/` directory is where target-dependent logic lives (`CodeGen` class, `EmitObject`, `EmitAssembly`).

**Proposed:** Move to `toolchain/codegen/codegen_options.h` / `toolchain/codegen/codegen_options.cpp`.

---

## 3. `CompilationUnit::RunOptimize()` → `toolchain/codegen/optimize.cpp`

**What:** ~60 lines (`compile_subcommand.cpp:670-730`) setting up the full LLVM New Pass Manager pipeline — `PassBuilder`, `PipelineTuningOptions`, analysis managers, loop vectorization/unrolling/interleaving configuration, `TargetLibraryInfo`.

**Why it's misplaced:** This is an LLVM IR optimization pass operating on `llvm::Module` post-lowering. It has no relation to CLI subcommand dispatch. It should be a standalone function like `RunLLVMOptimizationPipeline(module, target_machine, opt_level)`.

**Proposed:** Extract into `toolchain/codegen/optimize.h` / `toolchain/codegen/optimize.cpp`.

---

## 4. `CompilationUnit::MakeTargetMachine()` → `toolchain/codegen/`

**What:** ~17 lines (`compile_subcommand.cpp:638-654`) creating `llvm::TargetMachine` with CPU `"generic"`, `Reloc::PIC_`, `FunctionSections=true`, `DataSections=true`.

**Why it's misplaced:** Target machine creation is codegen infrastructure. The `CodeGen` class already takes a `TargetMachine*` — the factory that creates it should live alongside it.

**Proposed:** Extract as a factory function in `toolchain/codegen/`, e.g., `CreateTargetMachine(target, triple)`.

---

## 5. `CompilationUnit::RunCodeGenHelper()` — mixed concerns

**What:** ~130 lines (`compile_subcommand.cpp:762-892`) that handles:
1. Output format selection (object / assembly / bitcode)
2. File I/O (temp file creation, output file opening)
3. `CodeGen::Emit*()` calls
4. `LinkOptions` construction and `InvokeLinker()` call

**Why it's a problem:** This single driver method mixes three separate concerns: codegen emission, file management, and linker orchestration. The "emit to temp .o then link into executable" pattern should be a composable pipeline step, not embedded in the driver.

**Proposed:** Split into:
- Codegen emission stays in driver (thin orchestration)
- Link step calls into the extracted `toolchain/linker/` library

---

## 6. `lower/options.h::OptimizationLevel` — shared across layers

**What:** `Lower::OptimizationLevel` enum (`None`, `Debug`, `Size`, `Speed`) defined in `toolchain/lower/options.h`.

**Why it's questionable:** This enum is used by both lowering *and* the LLVM optimization pipeline (`GetLLVMOptimizationLevel()` in `compile_subcommand.cpp:656`). It maps to `llvm::OptimizationLevel` (O0/O1/Oz/O3). It arguably belongs at a shared level since it governs behavior across multiple pipeline stages.

**Proposed:** Consider moving to `toolchain/base/optimization_level.h` or `toolchain/codegen/`.

---

## 7. Dead includes in `codegen/codegen.cpp`

`codegen.cpp` has 7 includes that are unused — leftovers from when codegen likely did its own target machine setup before that logic moved to `compile_subcommand.cpp`.

| Include | Used? | Reason unused |
|---|---|---|
| `<memory>` | No | No smart pointers in this file |
| `<optional>` | No | No `std::optional` usage |
| `<string>` | No | No `std::string` usage |
| `llvm/MC/TargetRegistry.h` | No | No target lookup — `TargetMachine*` is passed in |
| `llvm/Target/TargetOptions.h` | No | No `TargetOptions` construction |
| `llvm/TargetParser/Host.h` | No | No host triple detection |
| `toolchain/diagnostics/consumer.h` | No | Already transitively included via `codegen.h` → `file_diagnostics.h` |

**Only needed includes:**
```cpp
#include "toolchain/codegen/codegen.h"
#include "common/check.h"
#include "llvm/Bitcode/BitcodeWriter.h"
#include "llvm/IR/LegacyPassManager.h"
```

---

## Proposed directory structure after refactor

```
toolchain/
├── codegen/
│   ├── codegen.h / codegen.cpp         (existing, cleaned up)
│   ├── codegen_options.h / .cpp         (moved from driver/)
│   ├── optimize.h / optimize.cpp        (extracted from compile_subcommand.cpp)
│   └── target_machine.h / .cpp          (extracted from compile_subcommand.cpp)
├── linker/
│   ├── link.h / link.cpp               (moved from driver/)
│   └── BUILD
├── driver/
│   ├── compile_subcommand.h / .cpp      (slimmed down — orchestration only)
│   ├── build_subcommand.h / .cpp
│   ├── ... (other subcommands)
│   └── BUILD                            (updated deps)
```

This brings the `driver/` in line with the principle that it should only contain CLI dispatch logic, while each pipeline stage (`lex/`, `parse/`, `check/`, `lower/`, `codegen/`, `linker/`) owns its own implementation.
