// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef TINYSWIFT_TOOLCHAIN_BASE_KIND_SWITCH_H_
#define TINYSWIFT_TOOLCHAIN_BASE_KIND_SWITCH_H_

#include <type_traits>

#include "llvm/ADT/STLExtras.h"
#include "toolchain/base/for_each_macro.h"

// This library provides switch-like behaviors for TinySwift's kind-based types.
//
// An expected use case is to mix regular switch `case` statements and
// `TINYSWIFT_KIND`. However, the `switch` must be defined using
// `TINYSWIFT_KIND_SWITCH`. For example:
//
//   TINYSWIFT_KIND_SWITCH(untyped_inst) {
//     case TINYSWIFT_KIND(SomeInstType inst): {
//       return inst.typed_field;
//     }
//     case OtherType1::Kind:
//     case OtherType2::Kind:
//       return value;
//     default:
//       return default_value;
//   }
//
// For compatibility, this requires:
//
// - The type passed to `TINYSWIFT_KIND_SWITCH` has `.kind()` to switch on, and
//   `.As<CaseT>` for `TINYSWIFT_KIND` to cast to.
// - Each type passed to `TINYSWIFT_KIND` (`CaseT` above) provides
//   `CaseT::Kind`, which is passed to the `case` keyword.
//   `CaseT::Kind::RawEnumType` is the type returned by `.kind()`.
//
// Note, this is currently used primarily for Inst in toolchain. When more
// use-cases are added, it would be worth considering whether the API
// requirements should change.
namespace TinySwift::Internal::Kind {

template <typename T>
constexpr bool IsStdVariantValue = false;

template <typename... Ts>
constexpr bool IsStdVariantValue<std::variant<Ts...>> = true;

template <typename T>
concept IsStdVariant = IsStdVariantValue<std::decay_t<T>>;

#define TINYSWIFT_INTERNAL_KIND_IDENTIFIER(name) T##name
// Turns a list of numbers into a list `T0, T1, ...`.
#define TINYSWIFT_INTERNAL_KIND_IDENTIFIERS(...)                             \
  TINYSWIFT_FOR_EACH(TINYSWIFT_INTERNAL_KIND_IDENTIFIER, TINYSWIFT_FOR_EACH_COMMA, \
                  __VA_ARGS__)

#define TINYSWIFT_INTERNAL_KIND_TYPENAME(name) \
  typename TINYSWIFT_INTERNAL_KIND_IDENTIFIER(name)
// Turns a list of numbers into a list `typename T0, typename T1, ...`.
#define TINYSWIFT_INTERNAL_KIND_TYPENAMES(...)                             \
  TINYSWIFT_FOR_EACH(TINYSWIFT_INTERNAL_KIND_TYPENAME, TINYSWIFT_FOR_EACH_COMMA, \
                  __VA_ARGS__)

#define TINYSWIFT_INTERNAL_KIND_ENUM_NAME(n) VariantType##n##NotHandledInSwitch
// Turns a list of numbers into a list `VariantType0NotHandledInSwitch, ...`.
#define TINYSWIFT_INTERNAL_KIND_TYPES_TO_ENUM_NAMES(...)                    \
  TINYSWIFT_FOR_EACH(TINYSWIFT_INTERNAL_KIND_ENUM_NAME, TINYSWIFT_FOR_EACH_COMMA, \
                  __VA_ARGS__)

// Turns a list of numbers into a set of template specializations of the
// variable `EnumType EnumValue`, with each specialization having the Nth value
// in the EnumType (as defined by TINYSWIFT_INTERNAL_KIND_TYPES_TO_ENUM_NAMES).
#define TINYSWIFT_INTERNAL_KIND_TYPE_TO_ENUM_NAME(n)                    \
  template <>                                                        \
  constexpr EnumType EnumValue<TINYSWIFT_INTERNAL_KIND_IDENTIFIER(n)> = \
      EnumType::TINYSWIFT_INTERNAL_KIND_ENUM_NAME(n)

// Used to provide a reason in the compiler error from `ValidCaseType`, which
// will state that "T does not satisfy TypeFoundInVariant".
template <typename T>
concept TypeFoundInVariant = false;

// Used to cause a compler error, which will state that "ValidCaseType was not
// satisfied" for T and std::variant<...>.
template <typename T, typename StdVariant>
  requires TypeFoundInVariant<T>
struct ValidCaseType;

template <typename T>
struct StdVariantTypeMap;

#define TINYSWIFT_INTERNAL_KIND_TYPE_MAP(...)                                     \
  template <TINYSWIFT_INTERNAL_KIND_TYPENAMES(__VA_ARGS__)>                       \
  struct StdVariantTypeMap<                                                    \
      std::variant<TINYSWIFT_INTERNAL_KIND_IDENTIFIERS(__VA_ARGS__)>> {           \
    /* An enum with a value for each type in the std::variant. The switch will \
     * be on this enum so that we get a warning if one of the enum values is   \
     * not handled. They are named in a way to help explain the warning, that  \
     * it means a type in the std::variant<...> type list does not have a      \
     * matching case statement.                                                \
     */                                                                        \
    enum class EnumType {                                                      \
      TINYSWIFT_INTERNAL_KIND_TYPES_TO_ENUM_NAMES(__VA_ARGS__)                    \
    };                                                                         \
    /* A mapping from a single type in the std::variant<...> type list to a    \
     * value in the EnumType. This value is only used in the case a type is    \
     * queried which is not part of the type list, and ValidCaseType is used   \
     * to produce a diagnostic explaining the situation.  */                   \
    template <typename Tn>                                                     \
    static constexpr EnumType EnumValue = ValidCaseType<                       \
        Tn, std::variant<TINYSWIFT_INTERNAL_KIND_IDENTIFIERS(__VA_ARGS__)>>();    \
    /**/                                                                       \
    TINYSWIFT_FOR_EACH(TINYSWIFT_INTERNAL_KIND_TYPE_TO_ENUM_NAME,                    \
                    TINYSWIFT_FOR_EACH_SEMI, __VA_ARGS__);                        \
  }

template <typename... Ts>
struct StdVariantTypeMap<std::variant<Ts...>> {
  // The number here should match the number of arguments in the largest
  // `TINYSWIFT_INTERNAL_KIND_TYPE_MAP` invocation below.
  static_assert(sizeof...(Ts) <= 24,
                "TINYSWIFT_KIND_SWITCH supports std::variant with up to 24 types. "
                "Add more if needed.");
};

// Generate StdVariantTypeMap specializations for each number of types in the
// std::variant<...> type list. The numbers here represent which number is
// printed in diagnostics stating a type in the variant has no matching case
// statement. Duplicate numbers would create an error.
TINYSWIFT_INTERNAL_KIND_TYPE_MAP(0);
TINYSWIFT_INTERNAL_KIND_TYPE_MAP(0, 1);
TINYSWIFT_INTERNAL_KIND_TYPE_MAP(0, 1, 2);
TINYSWIFT_INTERNAL_KIND_TYPE_MAP(0, 1, 2, 3);
TINYSWIFT_INTERNAL_KIND_TYPE_MAP(0, 1, 2, 3, 4);
TINYSWIFT_INTERNAL_KIND_TYPE_MAP(0, 1, 2, 3, 4, 5);
TINYSWIFT_INTERNAL_KIND_TYPE_MAP(0, 1, 2, 3, 4, 5, 6);
TINYSWIFT_INTERNAL_KIND_TYPE_MAP(0, 1, 2, 3, 4, 5, 6, 7);
TINYSWIFT_INTERNAL_KIND_TYPE_MAP(0, 1, 2, 3, 4, 5, 6, 7, 8);
TINYSWIFT_INTERNAL_KIND_TYPE_MAP(0, 1, 2, 3, 4, 5, 6, 7, 8, 9);
TINYSWIFT_INTERNAL_KIND_TYPE_MAP(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
TINYSWIFT_INTERNAL_KIND_TYPE_MAP(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11);
TINYSWIFT_INTERNAL_KIND_TYPE_MAP(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12);
TINYSWIFT_INTERNAL_KIND_TYPE_MAP(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13);
TINYSWIFT_INTERNAL_KIND_TYPE_MAP(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14);
TINYSWIFT_INTERNAL_KIND_TYPE_MAP(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
                              15);
TINYSWIFT_INTERNAL_KIND_TYPE_MAP(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
                              15, 16);
TINYSWIFT_INTERNAL_KIND_TYPE_MAP(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
                              15, 16, 17);
TINYSWIFT_INTERNAL_KIND_TYPE_MAP(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
                              15, 16, 17, 18);
TINYSWIFT_INTERNAL_KIND_TYPE_MAP(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
                              15, 16, 17, 18, 19);
TINYSWIFT_INTERNAL_KIND_TYPE_MAP(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
                              15, 16, 17, 18, 19, 20);
TINYSWIFT_INTERNAL_KIND_TYPE_MAP(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
                              15, 16, 17, 18, 19, 20, 21);
TINYSWIFT_INTERNAL_KIND_TYPE_MAP(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
                              15, 16, 17, 18, 19, 20, 21, 22);
TINYSWIFT_INTERNAL_KIND_TYPE_MAP(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
                              15, 16, 17, 18, 19, 20, 21, 22, 23);

#undef TINYSWIFT_INTERNAL_KIND_IDENTIFIER
#undef TINYSWIFT_INTERNAL_KIND_IDENTIFIERS
#undef TINYSWIFT_INTERNAL_KIND_TYPENAME
#undef TINYSWIFT_INTERNAL_KIND_TYPENAMES
#undef TINYSWIFT_INTERNAL_KIND_ENUM_NAME
#undef TINYSWIFT_INTERNAL_KIND_TYPES_TO_ENUM_NAMES
#undef TINYSWIFT_INTERNAL_KIND_TYPE_TO_ENUM_NAME
#undef TINYSWIFT_INTERNAL_KIND_TYPE_MAP

// Uses the above `TINYSWIFT_INTERNAL_KIND_TYPE_MAP` expansions to make an enum
// with a value for each type in a std::variant<...> type list.
template <typename StdVariant>
using StdVariantEnum = StdVariantTypeMap<std::decay_t<StdVariant>>::EnumType;

// Uses the `TINYSWIFT_INTERNAL_KIND_TYPE_MAP` expanstions to find the enum value
// in `StdVariantEnum` for a given type `T` in the type list of a
// std::variant<...>.
template <typename T, typename StdVariant>
constexpr auto CaseValueOfTypeInStdVariant =
    StdVariantTypeMap<std::decay_t<StdVariant>>::template EnumValue<T>;

// Given `TINYSWIFT_KIND_SWITCH(value)` this returns the actual value to switch on.
//
// For types with a `kind()` accessor, this is the just the value of `kind()`.
// The type returned from `kind()` is expected to be a `TypeEnum`, as it
// is required to have its API, including a nested `RawEnumType`.
//
// For std::variant<...> this is an enum synthesized from the types in the
// variant's type list.
template <typename SwitchT>
constexpr auto SwitchOn(SwitchT&& switch_value) -> auto {
  if constexpr (IsStdVariant<SwitchT>) {
    return static_cast<StdVariantEnum<SwitchT>>(switch_value.index());
  } else {
    return switch_value.kind();
  }
}

// Given `TINYSWIFT_KIND(CaseT name)` this generates the case value to compare
// against the switch value from `SwitchOn`.
//
// For types with a `kind()` accessor that returns a `TypeEnum`,
// this gets the `TypeEnum<...>::RawTypeEnum` for the case type `CaseT`.
//
// For std::variant<...> this returns the value corresponding to the case type
// from the enum synthesized (in `SwitchOn`) for the types in the variant's
// type list.
template <typename SwitchT, typename CaseFnT>
consteval auto ForCase() -> auto {
  using CaseT = llvm::function_traits<CaseFnT>::template arg_t<0>;
  if constexpr (IsStdVariant<SwitchT>) {
    using NoRefCaseT = std::remove_cvref_t<CaseT>;
    return CaseValueOfTypeInStdVariant<NoRefCaseT, SwitchT>;
  } else {
    using KindT = llvm::function_traits<
        decltype(&std::remove_cvref_t<SwitchT>::kind)>::result_t;
    return static_cast<KindT::RawEnumType>(KindT::template For<CaseT>);
  }
}

// Given `TINYSWIFT_KIND_SWITCH(value)` and `TINYSWIFT_KIND(CaseT name)` this converts
// the `value` to `CaseT`.
//
// For types with a `kind()` accessor this uses `value.As<CaseT>`.
//
// For `std::variant<...>` this uses `std::get<CaseT>(value)`.
template <typename CaseFnT, typename SwitchT>
auto Cast(SwitchT&& kind_switch_value) -> decltype(auto) {
  using CaseT = llvm::function_traits<CaseFnT>::template arg_t<0>;
  if constexpr (IsStdVariant<SwitchT>) {
    using NoRefCaseT = std::remove_cvref_t<CaseT>;
    return std::get<NoRefCaseT>(std::forward<SwitchT>(kind_switch_value));
  } else {
    return kind_switch_value.template As<CaseT>();
  }
}

#define TINYSWIFT_INTERNAL_KIND_MERGE(Prefix, Line) Prefix##Line
#define TINYSWIFT_INTERNAL_KIND_LABEL(Line) \
  TINYSWIFT_INTERNAL_KIND_MERGE(tinyswift_internal_kind_case_, Line)

}  // namespace TinySwift::Internal::Kind

// Produces a switch statement on value.kind().
#define TINYSWIFT_KIND_SWITCH(value)                       \
  switch (                                              \
      auto&& tinyswift_internal_kind_switch_value = value; \
      ::TinySwift::Internal::Kind::SwitchOn(tinyswift_internal_kind_switch_value))

// Produces a case-compatible block of code that also instantiates a local typed
// variable. typed_variable_decl looks like `int i`, with a space.
//
// This uses `if` to scope the variable, and provides a dangling `else` in order
// to prevent accidental `else` use. The label allows `:` to follow the macro
// name, making it look more like a typical `case`.
//
// Because of the dangling else, braces should always be used with a `case
// TINYSWIFT_KIND`. Otherwise, only the first statement is going to see the
// variable. Even if that sometimes works, it can lead to confusing issues when
// statements are added, and `if`/`else` coding style already requires braces.
#define TINYSWIFT_KIND(typed_variable_decl)                                   \
  ::TinySwift::Internal::Kind::ForCase<                                       \
      decltype(tinyswift_internal_kind_switch_value),                         \
      decltype([]([[maybe_unused]] typed_variable_decl) {})>()             \
      : if (typed_variable_decl = ::TinySwift::Internal::Kind::Cast<          \
                decltype([]([[maybe_unused]] typed_variable_decl) {})>(    \
                std::forward<decltype(tinyswift_internal_kind_switch_value)>( \
                    tinyswift_internal_kind_switch_value));                   \
            false) {}                                                      \
  else [[maybe_unused]] TINYSWIFT_INTERNAL_KIND_LABEL(__LINE__)

#endif  // TINYSWIFT_TOOLCHAIN_BASE_KIND_SWITCH_H_
