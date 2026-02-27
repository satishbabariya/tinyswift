// Part of the TinySwift Core Prelude.
// Time free functions.

@extern("C") func __tinyswift_clock_now() -> Int
@extern("C") func __tinyswift_clock_monotonic() -> Int

func clockNow() -> Int { return __tinyswift_clock_now() }
func clockMonotonic() -> Int { return __tinyswift_clock_monotonic() }
