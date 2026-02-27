// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef TINYSWIFT_TOOLCHAIN_CHECK_SYNTHESIS_H_
#define TINYSWIFT_TOOLCHAIN_CHECK_SYNTHESIS_H_

#include "toolchain/check/context.h"
#include "toolchain/check/handle_type_decl.h"
#include "toolchain/sem_ir/ids.h"

namespace TinySwift::Check {

// M109: Synthesize Equatable conformance (== and != operators).
void SynthesizeEquatable(Context& context, SemIR::InstId type_inst_id,
                         SemIR::NameScopeId scope_id,
                         llvm::ArrayRef<FieldInfo> fields,
                         bool is_enum, int enum_case_count);

// M109: Synthesize Comparable conformance (< operator).
void SynthesizeComparable(Context& context, SemIR::InstId type_inst_id,
                          SemIR::NameScopeId scope_id,
                          llvm::ArrayRef<FieldInfo> fields);

// M110: Synthesize Hashable conformance (hash() method).
void SynthesizeHashable(Context& context, SemIR::InstId type_inst_id,
                        SemIR::NameScopeId scope_id,
                        llvm::ArrayRef<FieldInfo> fields,
                        bool is_enum, int enum_case_count);

// M111: Synthesize CustomStringConvertible conformance (description property).
void SynthesizeDescription(Context& context, SemIR::InstId type_inst_id,
                           SemIR::NameScopeId scope_id,
                           llvm::ArrayRef<FieldInfo> fields,
                           llvm::StringRef type_name);

}  // namespace TinySwift::Check

#endif  // TINYSWIFT_TOOLCHAIN_CHECK_SYNTHESIS_H_
