# Plan: Move Bool to Prelude Based on Int1

## Goal
Remove `BoolType` as a singleton built-in type and redefine `Bool` as a prelude
typealias over `Int1` (the 1-bit signed integer already supported by the
compiler via the recent integer refactor). This aligns Bool with Swift's model
where Bool is a struct wrapping a 1-bit Builtin.Int1.

## Current State

### BoolType as a singleton
- `BoolType` is defined as a singleton in `sem_ir/singleton_insts.h` (index 1)
- It has a fixed `TypeInstId` and `ConstantId` derived from its singleton index
- `GetBuiltinType("Bool")` in `check/context.cpp` returns a TypeId based on
  this singleton
- `check/check.cpp` registers `"Bool"` → `BoolType::TypeInstId` in the package
  scope for extensions

### BoolType in checking (semantic analysis)
- `IsBoolType()` in `handle_expr.cpp` compares type_id against
  `GetBuiltinType("Bool")`
- All comparison operators produce `.type_id = bool_type` (the Bool TypeId)
- `BoolLiteral` instructions carry `.type_id = bool_type` with a `BoolValue`
  (True/False)
- `BoolNot`, `BoolAnd`, `BoolOr` instructions carry `.type_id = bool_type`
- `BranchIf` consumes a `cond_id` that must be Bool-typed (i1 at LLVM level)
- `handle_function.cpp:457` matches `BoolType` for method dispatch on `Bool`
- `handle_type_decl.cpp:1730` matches `BoolType` for `extension Bool { ... }`
- `synthesis.cpp` extensively creates `BoolLiteral`, `BoolAnd`, `BoolNot` for
  derived Equatable/Comparable/Hashable conformances

### BoolType in lowering
- `handle_type.cpp`: `BoolType` → `getInt1Ty()` (LLVM i1)
- `handle_inst.cpp`: `BoolLiteral` → `ConstantInt::get(i1, value)`
- `handle_inst.cpp`: `BoolNot` → `CreateNot`, `BoolAnd` → `CreateAnd`,
  `BoolOr` → `CreateOr`
- `BranchIf` passes the Bool value directly to `CreateCondBr` (expects i1)
- `context.cpp` debug info: `BoolType` → `createBasicType("Bool", 8,
  DW_ATE_boolean)`
- `lower.cpp` SIL path: uses `getInt1Ty()` for bool comparisons

### Optional type uses Bool
- `OptionalType` lowering creates `{i1, T}` struct using `getInt1Ty()`
  directly — this is the has_value flag, not BoolType

### Prelude Bool files
- `core/BoolExtensions.swift` — extension methods on Bool (hashValue,
  description)
- `min_prelude/parts/bool.swift` — `fn Bool() -> type = "bool.make_type"`

## Design: Bool = Int1 in the Prelude

### Phase 1: Make GetBuiltinType("Bool") return Int1

**File: `toolchain/check/context.cpp`**

Change:
```cpp
if (name == "Bool") {
    return SemIR::TypeId::ForTypeConstant(
        SemIR::ConstantId::ForConcreteConstant(SemIR::BoolType::TypeInstId));
}
```
To:
```cpp
if (name == "Bool") {
    return MakeIntType(SemIR::IntKind::Signed, 1);
}
```

This makes all `GetBuiltinType("Bool")` callers automatically get `IntType(Signed, 1)`.

### Phase 2: Update IsBoolType() to check for Int1

**File: `toolchain/check/handle_expr.cpp`**

Change `IsBoolType()` to check for `IntType` with bit_width=1 instead of
matching the `BoolType` singleton. Or simpler: just compare against
`GetBuiltinType("Bool")` which now returns Int1.

### Phase 3: Update BoolLiteral to emit IntValue

**File: `toolchain/check/handle_expr.cpp` + `synthesis.cpp`**

Replace all `SemIR::BoolLiteral{.type_id = bool_type, .value = ...}` with:
```cpp
SemIR::IntValue{.type_id = bool_type, .int_id = ints().Add(value ? 1 : 0)}
```

This eliminates the need for `BoolLiteral` as a separate instruction kind.

Alternatively (simpler intermediate step): keep `BoolLiteral` but give it the
Int1 type_id. Lowering already emits i1 constants for BoolLiteral — no change
needed there.

### Phase 4: Redefine BoolNot/BoolAnd/BoolOr (optional)

The operations `BoolNot`, `BoolAnd`, `BoolOr` are bitwise operations on i1.
They map cleanly to LLVM `not`/`and`/`or` on i1 values.

**Option A (keep them):** These instructions remain as-is, but their `type_id`
is now Int1 instead of BoolType. Lowering already emits `CreateNot`/
`CreateAnd`/`CreateOr` — no change needed.

**Option B (reuse IntNot/IntAnd/IntOr):** Add `IntNot`, `IntAnd`, `IntOr`
instructions and use them for all integer widths including i1. This is cleaner
long-term but more work.

**Recommendation: Option A for this PR.** Keep BoolNot/BoolAnd/BoolOr, just
change their type_id to Int1.

### Phase 5: Remove BoolType singleton

**File: `toolchain/sem_ir/singleton_insts.h`**

Remove `InstKind::BoolType` from `SingletonInstKinds`. This shifts indices of
all subsequent singletons (IntLiteralType, NamespaceType, TypeType, StringType,
FloatType, DoubleType).

**CRITICAL:** This is the highest-risk change because:
- Other singletons' `TypeInstId` values are derived from their array index
- Any code using `BoolType::TypeInstId` directly will break
- The singleton inst IDs for all subsequent types will shift by -1

**Mitigation:** Since we're replacing all `BoolType::TypeInstId` references
with `MakeIntType(Signed, 1)` in earlier phases, the singleton can be safely
removed after all references are eliminated.

### Phase 6: Update check.cpp scope registration

**File: `toolchain/check/check.cpp`**

Change the Bool entry in the `builtin_types[]` array. Since Bool is no longer a
singleton, we can't put it in the static array. Instead, register it after
context creation:
```cpp
// Register Bool as Int1 in the package scope.
auto bool_type = context.GetBuiltinType("Bool");
auto bool_inst_id = context.types().GetTypeInstId(bool_type);
context.AddNameToScope(SemIR::NameId::ForIdentifier(
    context.identifiers().Add("Bool")), bool_inst_id);
```

### Phase 7: Update handle_function.cpp and handle_type_decl.cpp

**File: `toolchain/check/handle_function.cpp:457`**

Replace `type_inst.Is<SemIR::BoolType>()` with a check for IntType with
bit_width=1, or use a helper `IsBoolType(context, type_id)`.

**File: `toolchain/check/handle_type_decl.cpp:1730`**

Same — replace `type_inst.Is<SemIR::BoolType>()` with IntType check or add
`IntType` to the list of built-in types that support extensions.

### Phase 8: Update lowering

**File: `toolchain/lower/handle_type.cpp`**

The `BoolType` case can be removed — `IntType` with bit_width=1 already lowers
to `getInt1Ty()` via our recent IntType lowering fix.

**File: `toolchain/lower/handle_inst.cpp`**

- `BoolLiteral` lowering stays (emits i1 constant) OR is replaced by IntValue
  lowering which now respects bit width.
- `BoolNot`/`BoolAnd`/`BoolOr` lowering stays as-is (they work on i1 values).

**File: `toolchain/lower/context.cpp`**

Remove the `BoolType` case from debug info — IntType with bit_width=1 will
emit `createBasicType("Int1", 1, DW_ATE_signed)`. Override this to emit
`"Bool"` with `DW_ATE_boolean` for better debugger experience.

**File: `toolchain/lower/lower.cpp:264`**

Remove `kind == SemIR::InstKind::BoolType` check.

### Phase 9: Update BranchIf

`BranchIf` passes its `cond_id` to LLVM's `CreateCondBr`, which expects i1.
Since Bool is now Int1 (which lowers to i1), this continues to work with no
changes.

### Phase 10: Update prelude files

**File: `core/BoolExtensions.swift`**

No change needed — `extension Bool { ... }` will resolve `Bool` via
`GetBuiltinType("Bool")` → Int1.

**File: `toolchain/testing/testdata/min_prelude/parts/bool.swift`**

Update to reflect the new reality:
```swift
// Bool is Int1 — a 1-bit integer type.
// `true` is 1, `false` is 0.
```

### Phase 11: Update GetTypeName

**File: `toolchain/check/context.cpp`**

Add special case in `GetTypeName`: if IntType has bit_width=1, return `"Bool"`
instead of `"Int1"`.

## Files Modified (summary)

| File | Change |
|------|--------|
| `check/context.cpp` | GetBuiltinType("Bool") → MakeIntType(Signed, 1); GetTypeName special-case Int1 → "Bool" |
| `check/context.h` | Add IsBoolType() helper (optional) |
| `check/handle_expr.cpp` | Update IsBoolType() to check IntType(1); BoolLiteral → IntValue (optional) |
| `check/handle_stmt.cpp` | No change (uses GetBuiltinType("Bool") which is updated) |
| `check/handle_function.cpp` | Replace BoolType check with IntType(1) check |
| `check/handle_type_decl.cpp` | Replace BoolType check with IntType(1) check |
| `check/synthesis.cpp` | No change (uses GetBuiltinType("Bool") which is updated) |
| `check/check.cpp` | Update Bool scope registration |
| `sem_ir/singleton_insts.h` | Remove BoolType from SingletonInstKinds |
| `sem_ir/typed_insts.h` | Keep BoolType alias (deprecated) or remove |
| `sem_ir/inst_kind.def` | Remove BoolType (if fully removing) |
| `lower/handle_type.cpp` | Remove BoolType case |
| `lower/handle_inst.cpp` | Keep BoolLiteral/BoolNot/BoolAnd/BoolOr as-is |
| `lower/context.cpp` | Update debug info for Bool |
| `lower/lower.cpp` | Remove BoolType reference |
| `core/IntegerTypes.swift` | Add Bool = Int1 documentation |

## Execution Order

1. Update `GetBuiltinType("Bool")` to return `MakeIntType(Signed, 1)` ← makes all callers use Int1
2. Update `IsBoolType()` to compare against Int1 type
3. Update `GetTypeName()` to show "Bool" for Int1
4. Update `check.cpp` scope registration for Bool
5. Update `handle_function.cpp` and `handle_type_decl.cpp` BoolType checks
6. Remove `BoolType` from `singleton_insts.h` and `inst_kind.def`
7. Remove `BoolType` case from `lower/handle_type.cpp`
8. Update debug info in `lower/context.cpp`
9. Clean up `lower/lower.cpp`
10. Update prelude files

## Risk Assessment

**Medium risk.** The BoolType singleton is deeply embedded — 100+ references.
However, most references go through `GetBuiltinType("Bool")` which is a single
point of change. The riskiest part is removing the singleton from the array
(shifts all subsequent indices). This should be done last, after all direct
`BoolType::TypeInstId` references are eliminated.
