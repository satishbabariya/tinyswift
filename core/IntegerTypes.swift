// Part of the TinySwift Core Prelude.
// Platform-width integer type aliases and fixed-width integer types.
//
// On 64-bit platforms, Int and UInt are aliases for Int64 and UInt64.
// On 32-bit platforms, Int and UInt are aliases for Int32 and UInt32.
//
// The compiler resolves Int, UInt, Int8, Int16, Int32, Int64, UInt8, UInt16,
// UInt32, UInt64 as built-in types. This file documents the mapping and
// provides a place for future conditional compilation.

// Fixed-width signed integer types:
//   Int8   —  8-bit signed integer
//   Int16  — 16-bit signed integer
//   Int32  — 32-bit signed integer
//   Int64  — 64-bit signed integer

// Fixed-width unsigned integer types:
//   UInt8  —  8-bit unsigned integer
//   UInt16 — 16-bit unsigned integer
//   UInt32 — 32-bit unsigned integer
//   UInt64 — 64-bit unsigned integer

// Platform-width integer types (resolved by the compiler):
//   Int  — signed integer, same width as a pointer (64-bit on 64-bit platforms)
//   UInt — unsigned integer, same width as a pointer (64-bit on 64-bit platforms)

// TODO: When conditional compilation is available, use:
//   #if arch(x86_64) || arch(arm64)
//     typealias Int = Int64
//     typealias UInt = UInt64
//   #else
//     typealias Int = Int32
//     typealias UInt = UInt32
//   #endif
