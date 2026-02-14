// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef TINYSWIFT_COMMON_ENUM_BASE_H_
#define TINYSWIFT_COMMON_ENUM_BASE_H_

#include <compare>
#include <type_traits>

#include "common/ostream.h"
#include "llvm/ADT/StringRef.h"

namespace TinySwift::Internal {

// CRTP-style base class used to define the common pattern of TinySwift enum-like
// classes. The result is a class with named constants similar to enumerators,
// but that are normal classes, can contain other methods, and support a `name`
// method and printing the enums. These even work in switch statements and
// support `case MyEnum::Name:`.
//
// It is specifically designed to compose with X-MACRO style `.def` files that
// stamp out all the enumerators.
//
// Users must be in the `TinySwift` namespace and should look like the following.
//
// In `my_kind.h`:
//   ```
//   TINYSWIFT_DEFINE_RAW_ENUM_CLASS(MyKind, uint8_t) {
//   #define TINYSWIFT_MY_KIND(Name) TINYSWIFT_RAW_ENUM_ENUMERATOR(Name)
//   #include ".../my_kind.def"
//   };
//
//   class MyKind : public TINYSWIFT_ENUM_BASE(MyKind) {
//    public:
//   #define TINYSWIFT_MY_KIND(Name) TINYSWIFT_ENUM_CONSTANT_DECL(Name)
//   #include ".../my_kind.def"
//
//     // OPTIONAL: To support converting to and from the underlying type of
//     // the enumerator, add these lines:
//     using EnumBase::AsInt;
//     using EnumBase::FromInt;
//
//     // OPTIONAL: To expose the ability to create an instance from the raw
//     // enumerator (for unusual use cases), add this:
//     using EnumBase::Make;
//
//     // Plus, anything else you wish to include.
//   };
//
//   #define TINYSWIFT_MY_KIND(Name) TINYSWIFT_ENUM_CONSTANT_DEFINITION(MyKind, Name)
//   #include ".../my_kind.def"
//   ```
//
// In `my_kind.cpp`:
//   ```
//   TINYSWIFT_DEFINE_ENUM_CLASS_NAMES(MyKind) {
//   #define TINYSWIFT_MY_KIND(Name) TINYSWIFT_ENUM_CLASS_NAME_STRING(Name)
//   #include ".../my_kind.def"
//   };
//   ```
//
// The result of the above:
// - An enum class (`RawEnumType`) defined in an `Internal` namespace with one
//   enumerator per call to TINYSWIFT_MY_KIND(Name) in `.../my_kind.def`, with name
//   `Name`. This won't generally be used directly, but may be needed for niche
//   use cases such as a template argument.
// - A type `MyKind` that extends `TinySwift::Internal::EnumBase`.
//   - `MyKind` includes all the public members of `EnumBase`, like `name` and
//     `Print`. For example, you might call `name()` to construct an error
//     message:
//     ```
//     auto ErrorMessage(MyKind k) -> std::string {
//       return k.name() + " not found";
//     }
//     ```
//   - `MyKind` includes all protected members of `EnumBase`, like `AsInt`,
//     `FromInt`. They will be part of the public API of `EnumBase` if they
//     were included in a `using` declaration.
//   - `MyKind` includes a member `static const MyKind Name;` per call to
//     `TINYSWIFT_MY_KIND(Name)` in `.../my_kind.def`. It will have the
//     corresponding value from `RawEnumType`. This is the primary way to create
//     an instance of `MyKind`. For example, it might be used like:
//     ```
//     ErrorMessage(MyKind::Name1);
//     ```
//   - `MyKind` includes an implicit conversion to the `RawEnumType`, returning
//     the value of a private field in `EnumBase`. This is used when writing a
//     `switch` statement, as in this example:
//     ```
//     auto MyFunction(MyKind k) -> void {
//       // Implicitly converts `k` and every `case` expression to
//       // `RawEnumType`:
//       switch (k) {
//         case MyKind::Name1:
//           // ...
//           break;
//
//         case MyKind::Name2:
//           // ...
//           break;
//
//         // No `default` case needed if the above cases are exhaustive.
//         // Prefer no `default` case when possible, to get an error if
//         // a case is skipped.
//       }
//     }
//     ```
//
template <typename DerivedT, typename EnumT, const llvm::StringLiteral Names[]>
class EnumBase : public Printable<DerivedT> {
 public:
  // An alias for the raw enum type. This is an implementation detail and
  // should rarely be used directly, only when an actual enum type is needed.
  using RawEnumType = EnumT;

  using EnumType = DerivedT;
  using UnderlyingType = std::underlying_type_t<RawEnumType>;

  // Enable conversion to the raw enum type, including in a `constexpr` context,
  // to enable comparisons and usage in `switch` and `case`. The enum type
  // remains an implementation detail and nothing else should be using this
  // function.
  //
  // NOLINTNEXTLINE(google-explicit-constructor)
  explicit(false) constexpr operator RawEnumType() const { return value_; }

  // Conversion to bool is deleted to prevent direct use in an `if` condition
  // instead of comparing with another value.
  explicit operator bool() const = delete;

  // Returns the name of this value.
  auto name() const -> llvm::StringRef { return Names[AsInt()]; }

  // Prints this value using its name.
  auto Print(llvm::raw_ostream& out) const -> void { out << name(); }

  // Don't support comparison of enums by default.
  friend auto operator<(DerivedT lhs, DerivedT rhs) -> bool = delete;
  friend auto operator<=(DerivedT lhs, DerivedT rhs) -> bool = delete;
  friend auto operator>(DerivedT lhs, DerivedT rhs) -> bool = delete;
  friend auto operator>=(DerivedT lhs, DerivedT rhs) -> bool = delete;
  friend auto operator<=>(DerivedT lhs, DerivedT rhs)
      -> std::partial_ordering = delete;

 protected:
  // The default constructor is explicitly defaulted (and constexpr) as a
  // protected constructor to allow derived classes to be constructed but not
  // the base itself. This should only be used in the `Make` function below.
  constexpr EnumBase() = default;

  // Create an instance from the raw enumerator. Mainly used internally, but may
  // be exposed for unusual use cases.
  static constexpr auto Make(RawEnumType value) -> EnumType {
    EnumType result;
    result.value_ = value;
    return result;
  }

  // Convert to the underlying integer type. Derived types can choose to expose
  // this as part of their API.
  constexpr auto AsInt() const -> UnderlyingType {
    return static_cast<UnderlyingType>(value_);
  }

  // Convert from the underlying integer type. Derived types can choose to
  // expose this as part of their API.
  static constexpr auto FromInt(UnderlyingType value) -> EnumType {
    return Make(static_cast<RawEnumType>(value));
  }

 private:
  template <typename MaskDerivedT, typename MaskEnumT,
            const llvm::StringLiteral MaskNames[]>
  friend class EnumMaskBase;

  RawEnumType value_;
};

}  // namespace TinySwift::Internal

// Use this before defining a class that derives from `EnumBase` to begin the
// definition of the raw `enum class`. It should be followed by the body of that
// raw enum class.
#define TINYSWIFT_DEFINE_RAW_ENUM_CLASS(EnumClassName, UnderlyingType) \
  namespace Internal {                                              \
  struct EnumClassName##Data {                                      \
    static const llvm::StringLiteral Names[];                       \
    enum class RawEnum : UnderlyingType;                            \
  };                                                                \
  }                                                                 \
  enum class Internal::EnumClassName##Data::RawEnum : UnderlyingType

// In the `TINYSWIFT_DEFINE_RAW_ENUM_CLASS` block, use this to generate each
// enumerator.
#define TINYSWIFT_RAW_ENUM_ENUMERATOR(Name) Name,

// Use this to compute the `Internal::EnumBase` specialization for a TinySwift enum
// class. It both computes the name of the raw enum and ensures all the
// namespaces are correct.
#define TINYSWIFT_ENUM_BASE(EnumClassName)                                \
  ::TinySwift::Internal::EnumBase<EnumClassName,                          \
                               Internal::EnumClassName##Data::RawEnum, \
                               Internal::EnumClassName##Data::Names>

// Use this within the TinySwift enum class body to generate named constant
// declarations for each value.
#define TINYSWIFT_ENUM_CONSTANT_DECL(Name) static const EnumType Name;

// Use this immediately after the TinySwift enum class body to define each named
// constant.
#define TINYSWIFT_ENUM_CONSTANT_DEFINITION(EnumClassName, Name) \
  inline constexpr EnumClassName EnumClassName::Name =       \
      EnumClassName::Make(RawEnumType::Name);

// Use this in the `.cpp` file for an enum class to start the definition of the
// constant names array for each enumerator. It is followed by the desired
// constant initializer.
//
// `clang-format` has a bug with spacing around `->` returns in macros. See
// https://bugs.llvm.org/show_bug.cgi?id=48320 for details.
#define TINYSWIFT_DEFINE_ENUM_CLASS_NAMES(EnumClassName) \
  constexpr llvm::StringLiteral Internal::EnumClassName##Data::Names[] =

// Use this within the names array initializer to generate a string for each
// name.
#define TINYSWIFT_ENUM_CLASS_NAME_STRING(Name) #Name,

#endif  // TINYSWIFT_COMMON_ENUM_BASE_H_
