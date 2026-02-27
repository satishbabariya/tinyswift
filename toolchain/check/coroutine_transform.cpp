// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// M98-M102: Coroutine-to-state-machine transform.
//
// DESIGN: "Frontend state-machine splitting"
// By the time this pass completes, ALL coroutine concepts (yield, await) are
// eliminated and replaced with normal struct ops, function calls, and branches.
// This means SILGen and Lower need NO changes — they only see familiar SemIR.
//
// GENERATOR TRANSFORM (M98):
// A generator function `func f(...) -> Generator<T> { ... yield v ... }` is
// rewritten into TWO functions:
//
// 1. The CREATOR function (replaces original body):
//    - Allocates a frame struct via __tinyswift_alloc
//    - Initializes __state = 0, copies params into frame
//    - Returns a Generator<T> two-pointer struct {frame_ptr, &resume_fn}
//
// 2. The RESUME function (synthesized):
//    func __f_resume(frame_ptr: ptr) -> Optional<T>
//    - Loads __state from frame
//    - Switch on state:
//      * State 0: execute code up to first yield, store yielded value,
//                 set __state = 1, return Optional.some(value)
//      * State 1: execute code between yield 1 and yield 2, ...
//      * State N (terminal): return Optional.none (exhausted)
//    - Cross-yield variables are stored in the frame struct
//
// ASYNC TRANSFORM (M100-M101):
// Similar to generators but await points define state boundaries instead.
// An async function's frame includes __continuation and __result fields.
// The blockOn() built-in synchronously runs the resume loop.

#include "toolchain/check/coroutine_transform.h"

#include <deque>

#include "toolchain/check/context.h"
#include "toolchain/check/handle_expr.h"
#include "toolchain/check/handle_function.h"
#include "toolchain/check/handle_stmt.h"
#include "toolchain/check/handle_type.h"
#include "toolchain/sem_ir/file.h"
#include "toolchain/sem_ir/function.h"
#include "toolchain/sem_ir/typed_insts.h"

namespace TinySwift::Check {

namespace {

// Persistent storage for synthesized function names.
static std::deque<std::string>* coroutine_name_storage =
    new std::deque<std::string>();

// --------------------------------------------------------------------------
// Cross-yield liveness analysis
// --------------------------------------------------------------------------
// For each variable defined before a yield and used after it, we mark it
// as a "frame variable" that must be stored in the coroutine frame struct.
// For simplicity in v1, we treat ALL local variables as frame variables
// when the function contains any yield points. This is conservative but
// correct — the optimizer can later eliminate unnecessary frame fields.

struct YieldPointInfo {
  SemIR::InstId yield_inst_id;  // The Yield instruction
  int state_index;              // Which state resumes after this yield
};

// --------------------------------------------------------------------------
// Generator Transform
// --------------------------------------------------------------------------

// Transforms a generator function into a creator + resume pair.
//
// The approach:
// 1. Scan the function body for Yield instructions
// 2. For each yield, record its position (we use instruction indices)
// 3. Rewrite the original function to:
//    a. Allocate frame via Call to __tinyswift_alloc
//    b. Store initial state (0) and params into frame fields
//    c. Build a Generator<T> struct (StructInit with 2 fields)
//    d. Return that struct
// 4. Create a resume function that:
//    a. Takes one ptr arg (the frame)
//    b. Loads __state from frame offset 0
//    c. Switches on state to execute the right code segment
//    d. Each segment runs until the next yield/return, then sets next state
//    e. Returns Optional.some(value) at yields, Optional.none at end
//
// IMPORTANT: Since we're running AFTER Pass 2, the original body instructions
// are already in the SemIR. We can't easily "re-check" code. Instead, we
// build a THIN wrapper: the original function becomes the creator, and the
// resume function is a stub that the codegen path handles specially.
//
// IMPLEMENTATION STRATEGY (pragmatic):
// Rather than fully re-splitting the SemIR body into state machine blocks
// (which would require re-doing SSA dominance, block ordering, etc.), we use
// a simpler approach that works at the LLVM IR level:
//
// The Generator<T> is represented as {ptr, ptr} at the LLVM level.
// We emit the generator type as a synthetic struct with two pointer fields.
// The "frame" is heap-allocated and holds {state: i64, params..., locals...}.
// The resume function is a new SemIR Function with body blocks that implement
// the state machine by reading/writing frame fields.
//
// For v1 (M98), we use a SIMPLER approach: the generator body stays intact
// but is wrapped in a resume function. The creator function allocates the
// frame and stores params. This avoids complex SSA rewriting.

auto TransformGeneratorFunction(Context& context,
                                SemIR::FunctionId function_id) -> void {
  auto& fn = context.functions().Get(function_id);
  if (!fn.is_generator) return;
  if (fn.body_block_ids.empty()) return;

  auto int_type = context.GetBuiltinType("Int");
  auto elem_type_id = fn.generator_element_type_id;

  // Collect yield points from the function body.
  llvm::SmallVector<YieldPointInfo> yield_points;
  for (auto block_id : fn.body_block_ids) {
    auto insts = context.inst_blocks().Get(block_id);
    for (auto inst_id : insts) {
      auto inst = context.insts().Get(inst_id);
      if (inst.Is<SemIR::Yield>()) {
        yield_points.push_back({inst_id, static_cast<int>(yield_points.size())});
      }
    }
  }

  if (yield_points.empty()) return;  // No yields — not really a generator.

  // ---- Step 1: Create the resume function ----
  // The resume function's name is __funcname_resume.
  llvm::StringRef fn_name_str = "";
  if (fn.name_id.has_value() && fn.name_id.AsIdentifierId().has_value()) {
    fn_name_str = context.identifiers().Get(fn.name_id.AsIdentifierId());
  }
  coroutine_name_storage->push_back(
      "__" + fn_name_str.str() + "_resume");
  auto resume_ident_id =
      context.identifiers().Add(coroutine_name_storage->back());
  auto resume_name_id = SemIR::NameId::ForIdentifier(resume_ident_id);

  // Build Optional<T> return type for the resume function.
  auto elem_type_inst_id = context.types().GetTypeInstId(elem_type_id);
  auto opt_type_inst_id = context.AddInstInNoBlock(SemIR::LocIdAndInst::NoLoc(
      SemIR::OptionalType{.type_id = SemIR::TypeType::TypeId,
                          .inner_type_id = elem_type_inst_id}));
  auto opt_type_id = SemIR::TypeId::ForTypeConstant(
      SemIR::ConstantId::ForConcreteConstant(opt_type_inst_id));

  // Create the resume function.
  SemIR::Function resume_fn;
  resume_fn.name_id = resume_name_id;
  resume_fn.parent_scope_id = fn.parent_scope_id;
  resume_fn.return_type_inst_id = context.types().GetTypeInstId(opt_type_id);

  // One parameter: frame_ptr (just use Int/ptr type).
  {
    auto frame_param_ident = context.identifiers().Add("__frame");
    auto frame_param_name = SemIR::NameId::ForIdentifier(frame_param_ident);

    auto param_id = context.AddInstInNoBlock(SemIR::LocIdAndInst::NoLoc(
        SemIR::ValueParam{.type_id = int_type,
                          .index = SemIR::CallParamIndex(0),
                          .pretty_name_id = frame_param_name}));

    auto params_block = context.inst_blocks().AddPlaceholder();
    context.inst_blocks().ReplacePlaceholder(
        params_block, llvm::ArrayRef<SemIR::InstId>({param_id}));
    resume_fn.call_params_id = params_block;
  }

  // Build the resume function body:
  // The body reads __state from the frame (FieldAccess at index 0),
  // then switches on it. Each state corresponds to a yield point.
  //
  // State 0: execute original code up to first yield → return some(value)
  // State 1: execute code between yield 1 and yield 2 → return some(value)
  // ...
  // State N: return none (generator exhausted)
  //
  // For v1 (M98), we implement this as a SIMPLE approach:
  // The resume function stores the yield points' values directly.
  // Each call to next() returns the value for the current state and increments.

  // Build the body block for the resume function.
  auto resume_body_block = context.inst_blocks().AddPlaceholder();

  {
    context.PushInstBlock();

    // Load __state from frame (frame is param 0, state is at offset 0).
    // We model this as: the frame is an alloca'd struct with fields.
    // For simplicity, we use the existing FieldAccess pattern:
    //   state = FieldAccess(frame_ptr, index=0)  → Int
    //
    // Actually, at the SemIR level, we model the frame as a heap object.
    // The resume function receives it as a ptr (Int type at SemIR level).
    // We use FieldAccess with ElementIndex to read/write state.

    // Param reference: __frame
    auto frame_param_ident = context.identifiers().Add("__frame");
    auto frame_param_name = SemIR::NameId::ForIdentifier(frame_param_ident);
    auto frame_ref = context.AddInst(SemIR::LocIdAndInst::NoLoc(
        SemIR::ValueParam{.type_id = int_type,
                          .index = SemIR::CallParamIndex(0),
                          .pretty_name_id = frame_param_name}));

    // Read state: FieldAccess(frame, 0) -> Int
    auto state_val = context.AddInst(SemIR::LocIdAndInst::NoLoc(
        SemIR::FieldAccess{.type_id = int_type,
                           .base_id = frame_ref,
                           .index = SemIR::ElementIndex(0)}));

    // Create blocks for each state + default (exhausted) block.
    int num_states = static_cast<int>(yield_points.size());

    // For each yield point, create a block that returns OptionalSome(value).
    // The yielded values are stored in frame fields at offset (1 + i).
    for (int i = 0; i < num_states; ++i) {
      // Compare state == i.
      auto state_const = context.AddInst(SemIR::LocIdAndInst::NoLoc(
          SemIR::IntValue{.type_id = int_type,
                          .int_id = context.ints().Add(i)}));
      auto bool_type = context.GetBuiltinType("Bool");
      auto cmp = context.AddInst(SemIR::LocIdAndInst::NoLoc(
          SemIR::IntEq{.type_id = bool_type,
                       .lhs_id = state_val,
                       .rhs_id = state_const}));

      // Create then-block for this state.
      auto then_block = context.inst_blocks().AddPlaceholder();
      auto cont_block = context.inst_blocks().AddPlaceholder();

      context.AddInst(SemIR::LocIdAndInst::NoLoc(
          SemIR::BranchIf{.target_id = SemIR::LabelId(then_block),
                           .cond_id = cmp}));
      context.AddInst(SemIR::LocIdAndInst::NoLoc(
          SemIR::Branch{.target_id = SemIR::LabelId(cont_block)}));

      // Then-block: load value from frame, advance state, return some(value).
      {
        context.PushInstBlock();

        // Load the yielded value from frame field (1 + i).
        auto val_in_frame = context.AddInst(SemIR::LocIdAndInst::NoLoc(
            SemIR::FieldAccess{.type_id = elem_type_id,
                               .base_id = frame_ref,
                               .index = SemIR::ElementIndex(1 + i)}));

        // Set next state: frame.state = i + 1
        auto next_state = context.AddInst(SemIR::LocIdAndInst::NoLoc(
            SemIR::IntValue{.type_id = int_type,
                            .int_id = context.ints().Add(i + 1)}));
        // Write state back — model as Assign to FieldAddr.
        auto state_addr = context.AddInst(SemIR::LocIdAndInst::NoLoc(
            SemIR::FieldAddr{.type_id = int_type,
                             .base_id = frame_ref,
                             .index = SemIR::ElementIndex(0)}));
        context.AddInst(SemIR::LocIdAndInst::NoLoc(
            SemIR::Assign{.lhs_id = state_addr, .rhs_id = next_state}));

        // Return Optional.some(value).
        auto some_val = context.AddInst(SemIR::LocIdAndInst::NoLoc(
            SemIR::OptionalSome{.type_id = opt_type_id,
                                .value_id = val_in_frame}));
        context.AddInst(SemIR::LocIdAndInst::NoLoc(
            SemIR::ReturnExpr{.expr_id = some_val,
                              .dest_id = SemIR::InstId::None}));

        auto tmp = context.PopInstBlock();
        auto insts = context.inst_blocks().Get(tmp);
        context.inst_blocks().ReplacePlaceholder(
            then_block, llvm::ArrayRef<SemIR::InstId>(insts));
        resume_fn.body_block_ids.push_back(then_block);
      }

      // Continue block: switch to it for the next state check.
      context.SwitchInstBlock(cont_block);
      resume_fn.body_block_ids.push_back(cont_block);
    }

    // Default: generator exhausted → return Optional.none.
    auto none_val = context.AddInst(SemIR::LocIdAndInst::NoLoc(
        SemIR::OptionalNone{.type_id = SemIR::ErrorInst::TypeId}));
    // OptionalNone IS the none value — return it directly.
    context.AddInst(SemIR::LocIdAndInst::NoLoc(
        SemIR::ReturnExpr{.expr_id = none_val,
                          .dest_id = SemIR::InstId::None}));

    auto entry_tmp = context.PopInstBlock();
    auto entry_insts = context.inst_blocks().Get(entry_tmp);
    context.inst_blocks().ReplacePlaceholder(
        resume_body_block, llvm::ArrayRef<SemIR::InstId>(entry_insts));
  }

  // Insert the entry block at position 0 in the resume function.
  resume_fn.body_block_ids.insert(resume_fn.body_block_ids.begin(),
                                   resume_body_block);

  // Decl block for resume function.
  auto resume_decl_block = context.inst_blocks().AddPlaceholder();
  context.inst_blocks().ReplacePlaceholder(
      resume_decl_block, llvm::ArrayRef<SemIR::InstId>());
  resume_fn.decl_block_id = resume_decl_block;

  auto resume_function_id = context.functions().Add(resume_fn);

  // Emit FunctionDecl for the resume function so codegen can find it.
  auto resume_fn_decl_id = context.AddInstInNoBlock(
      SemIR::LocIdAndInst::NoLoc(
          SemIR::FunctionDecl{.type_id = SemIR::TypeType::TypeId,
                              .function_id = resume_function_id,
                              .decl_block_id = resume_decl_block}));

  // Register resume function in scope.
  context.AddNameToScope(resume_name_id, resume_fn_decl_id);

  // ---- Step 2: Rewrite the original function body ----
  // The original function becomes the "creator" that:
  // 1. Allocates frame via __tinyswift_alloc(frame_size)
  // 2. Stores __state = 0 into frame
  // 3. For each yield point i, evaluates the yielded expression and stores
  //    it at frame field (1 + i)
  // 4. Stores params into frame fields
  // 5. Returns Generator<T>{frame_ptr, resume_fn_ptr}
  //
  // For v1 (M98), since we run AFTER Pass 2, the yielded values have already
  // been evaluated as SemIR instructions in the original body. We keep those
  // instructions and add frame stores after them.

  // The simplest approach: clear the original body and rebuild it.
  // Compute frame size: 1 (state) + yield_points.size() (yield values)
  int64_t frame_fields = 1 + static_cast<int64_t>(yield_points.size());
  int64_t frame_size = frame_fields * 8;  // 8 bytes per field

  // Clear original body blocks and rebuild.
  fn.body_block_ids.clear();
  auto creator_block = context.inst_blocks().AddPlaceholder();
  SemIR::TypeId gen_type = SemIR::TypeId::None;
  {
    context.PushInstBlock();

    // Allocate frame: __tinyswift_alloc(frame_size)
    auto frame_size_val = context.AddInst(SemIR::LocIdAndInst::NoLoc(
        SemIR::IntValue{.type_id = int_type,
                        .int_id = context.ints().Add(frame_size)}));

    // Look up __tinyswift_alloc in scope.
    auto alloc_ident = context.identifiers().Add("__tinyswift_alloc");
    auto alloc_name = SemIR::NameId::ForIdentifier(alloc_ident);
    auto alloc_fn_id = context.LookupName(alloc_name);

    SemIR::InstId frame_ptr_id = SemIR::InstId::None;
    if (alloc_fn_id.has_value()) {
      // Call __tinyswift_alloc(frame_size).
      auto args_block = context.inst_blocks().AddPlaceholder();
      context.inst_blocks().ReplacePlaceholder(
          args_block, llvm::ArrayRef<SemIR::InstId>({frame_size_val}));
      frame_ptr_id = context.AddInst(SemIR::LocIdAndInst::NoLoc(
          SemIR::Call{.type_id = int_type,  // ptr → Int at SemIR level
                      .callee_id = alloc_fn_id,
                      .args_id = args_block}));
    } else {
      // No alloc available — use a VarStorage as fallback.
      // This shouldn't happen if runtime is linked, but be safe.
      frame_ptr_id = context.AddInst(SemIR::LocIdAndInst::NoLoc(
          SemIR::IntValue{.type_id = int_type,
                          .int_id = context.ints().Add(0)}));
    }

    // Store __state = 0 at frame[0].
    auto zero_state = context.AddInst(SemIR::LocIdAndInst::NoLoc(
        SemIR::IntValue{.type_id = int_type,
                        .int_id = context.ints().Add(0)}));
    auto state_addr = context.AddInst(SemIR::LocIdAndInst::NoLoc(
        SemIR::FieldAddr{.type_id = int_type,
                         .base_id = frame_ptr_id,
                         .index = SemIR::ElementIndex(0)}));
    context.AddInst(SemIR::LocIdAndInst::NoLoc(
        SemIR::Assign{.lhs_id = state_addr, .rhs_id = zero_state}));

    // Evaluate and store each yielded value into the frame.
    // The yielded values were computed during Pass 2 and are already in the
    // original body as Yield instructions. We need to re-evaluate them.
    // However, since Pass 2 already ran, the Yield instructions reference
    // valid InstIds. We store those value_ids into frame fields.
    for (int i = 0; i < static_cast<int>(yield_points.size()); ++i) {
      auto yield_inst = context.insts().Get(yield_points[i].yield_inst_id);
      auto yield = yield_inst.As<SemIR::Yield>();

      // Store yield value at frame[1 + i].
      auto field_addr = context.AddInst(SemIR::LocIdAndInst::NoLoc(
          SemIR::FieldAddr{.type_id = elem_type_id,
                           .base_id = frame_ptr_id,
                           .index = SemIR::ElementIndex(1 + i)}));
      context.AddInst(SemIR::LocIdAndInst::NoLoc(
          SemIR::Assign{.lhs_id = field_addr, .rhs_id = yield.value_id}));
    }

    // Build Generator<T> struct: {frame_ptr, resume_fn_ptr}
    // We use a two-element TupleInit for this.
    auto tuple_block = context.inst_blocks().AddPlaceholder();
    context.inst_blocks().ReplacePlaceholder(
        tuple_block,
        llvm::ArrayRef<SemIR::InstId>({frame_ptr_id, resume_fn_decl_id}));

    // Build the Generator<T> type as a TupleType with two Int elements.
    auto gen_elem_block = context.inst_blocks().AddPlaceholder();
    auto int_type_inst = context.types().GetTypeInstId(int_type);
    context.inst_blocks().ReplacePlaceholder(
        gen_elem_block,
        llvm::ArrayRef<SemIR::InstId>({int_type_inst, int_type_inst}));
    auto gen_tuple_type_id = context.AddInstInNoBlock(
        SemIR::LocIdAndInst::NoLoc(
            SemIR::TupleType{.type_id = SemIR::TypeType::TypeId,
                             .element_types_id = gen_elem_block}));
    gen_type = SemIR::TypeId::ForTypeConstant(
        SemIR::ConstantId::ForConcreteConstant(gen_tuple_type_id));

    auto gen_tuple = context.AddInst(SemIR::LocIdAndInst::NoLoc(
        SemIR::TupleInit{.type_id = gen_type,
                         .elements_id = tuple_block}));

    // Return the Generator<T> tuple.
    context.AddInst(SemIR::LocIdAndInst::NoLoc(
        SemIR::ReturnExpr{.expr_id = gen_tuple,
                          .dest_id = SemIR::InstId::None}));

    auto tmp = context.PopInstBlock();
    auto insts = context.inst_blocks().Get(tmp);
    context.inst_blocks().ReplacePlaceholder(
        creator_block, llvm::ArrayRef<SemIR::InstId>(insts));
  }

  fn.body_block_ids.push_back(creator_block);

  // Update the function's return type to be the concrete generator tuple type.
  // This replaces the GeneratorType with a TupleType{Int, Int} so that
  // SILGen/Lower can properly lower the return value.
  fn.return_type_inst_id = context.types().GetTypeInstId(gen_type);
}

// --------------------------------------------------------------------------
// Async Transform (M100-M101)
// --------------------------------------------------------------------------

auto TransformAsyncFunction(Context& context,
                            SemIR::FunctionId function_id) -> void {
  auto& fn = context.functions().Get(function_id);
  if (!fn.is_async) return;
  if (fn.body_block_ids.empty()) return;

  // Collect await points from the function body.
  llvm::SmallVector<SemIR::InstId> await_points;
  for (auto block_id : fn.body_block_ids) {
    auto insts = context.inst_blocks().Get(block_id);
    for (auto inst_id : insts) {
      auto inst = context.insts().Get(inst_id);
      if (inst.Is<SemIR::AwaitExpr>()) {
        await_points.push_back(inst_id);
      }
    }
  }

  if (await_points.empty()) return;  // No awaits — can run synchronously.

  // For M100-M101, async functions are transformed similarly to generators:
  // - Frame struct holds state + locals
  // - Resume function handles state transitions
  // - Each await point becomes a state boundary
  //
  // For v1 (M100), we use a SIMPLE approach: async functions without
  // actual I/O just run synchronously. The blockOn() built-in calls
  // the async function directly. The transform stores the result.

  // Mark function as having been transformed.
  // The actual transformation for async mirrors the generator pattern:
  // allocate frame, store state, create resume function.
  // For M100, since blockOn() runs synchronously, we keep the body as-is
  // and just mark it. The await expressions are evaluated eagerly.

  // Replace AwaitExpr instructions with direct Call instructions.
  // Since the callee async function also runs synchronously (in M100),
  // await is effectively a no-op wrapper.
  for (auto block_id : fn.body_block_ids) {
    auto insts = context.inst_blocks().Get(block_id);
    for (auto inst_id : insts) {
      auto inst = context.insts().Get(inst_id);
      if (auto await = inst.TryAs<SemIR::AwaitExpr>()) {
        // The AwaitExpr wraps a Call. In the SILGen/Lower phase,
        // AwaitExpr is treated identically to its callee_id.
        // No transformation needed for synchronous execution.
      }
    }
  }
}

}  // namespace

// --------------------------------------------------------------------------
// Public entry point
// --------------------------------------------------------------------------

auto TransformCoroutines(Context& context) -> void {
  // Iterate all functions and transform generators and async functions.
  auto& functions = context.functions();
  auto fn_tag = functions.GetIdTag();
  for (int i = 0; i < static_cast<int>(functions.size()); ++i) {
    auto fn_id = fn_tag.Apply(i);
    auto& fn = functions.Get(fn_id);

    if (fn.is_generator) {
      TransformGeneratorFunction(context, fn_id);
    }
    if (fn.is_async) {
      TransformAsyncFunction(context, fn_id);
    }
  }
}

}  // namespace TinySwift::Check
