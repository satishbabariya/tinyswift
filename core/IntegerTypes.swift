// Part of the TinySwift Core Prelude.
// Integer type definitions — all resolved as compiler built-in types.
//
// The compiler resolves these type names via GetBuiltinType() and registers
// them in the package scope in check.cpp. No typealias declarations are
// needed; the names are intrinsically known to the compiler.
//
// Type mapping:
//   Bool  — IntType(Signed, 1)   — 1-bit integer, true=1, false=0
//   Int   — IntType(Signed, 64)  — platform-width signed integer
//   UInt  — IntType(Unsigned, 64) — platform-width unsigned integer
//   Int8  — IntType(Signed, 8)
//   Int16 — IntType(Signed, 16)
//   Int32 — IntType(Signed, 32)
//   Int64 — IntType(Signed, 64)
//   UInt8  — IntType(Unsigned, 8)
//   UInt16 — IntType(Unsigned, 16)
//   UInt32 — IntType(Unsigned, 32)
//   UInt64 — IntType(Unsigned, 64)
//
// Future: When conditional compilation is available, platform-width types
// will be defined via typealias based on target architecture:
//   #if arch(x86_64) || arch(arm64)
//     typealias Int = Int64
//     typealias UInt = UInt64
//   #else
//     typealias Int = Int32
//     typealias UInt = UInt32
//   #endif
