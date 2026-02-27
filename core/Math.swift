// Part of the TinySwift Core Prelude.
// Math free functions — thin wrappers around libm via the C runtime.

@extern("C") func __tinyswift_sqrt(_ x: Double) -> Double
@extern("C") func __tinyswift_pow(_ base: Double, _ exp: Double) -> Double
@extern("C") func __tinyswift_log(_ x: Double) -> Double
@extern("C") func __tinyswift_log2(_ x: Double) -> Double
@extern("C") func __tinyswift_log10(_ x: Double) -> Double
@extern("C") func __tinyswift_floor(_ x: Double) -> Double
@extern("C") func __tinyswift_ceil(_ x: Double) -> Double
@extern("C") func __tinyswift_round(_ x: Double) -> Double
@extern("C") func __tinyswift_sin(_ x: Double) -> Double
@extern("C") func __tinyswift_cos(_ x: Double) -> Double
@extern("C") func __tinyswift_tan(_ x: Double) -> Double
@extern("C") func __tinyswift_atan2(_ y: Double, _ x: Double) -> Double
@extern("C") func __tinyswift_fabs(_ x: Double) -> Double
@extern("C") func __tinyswift_fmod(_ x: Double, _ y: Double) -> Double

func sqrt(_ x: Double) -> Double { return __tinyswift_sqrt(x) }
func pow(_ base: Double, _ exp: Double) -> Double { return __tinyswift_pow(base, exp) }
func log(_ x: Double) -> Double { return __tinyswift_log(x) }
func log2(_ x: Double) -> Double { return __tinyswift_log2(x) }
func log10(_ x: Double) -> Double { return __tinyswift_log10(x) }
func floor(_ x: Double) -> Double { return __tinyswift_floor(x) }
func ceil(_ x: Double) -> Double { return __tinyswift_ceil(x) }
func round(_ x: Double) -> Double { return __tinyswift_round(x) }
func sin(_ x: Double) -> Double { return __tinyswift_sin(x) }
func cos(_ x: Double) -> Double { return __tinyswift_cos(x) }
func tan(_ x: Double) -> Double { return __tinyswift_tan(x) }
func atan2(_ y: Double, _ x: Double) -> Double { return __tinyswift_atan2(y, x) }
func fabs(_ x: Double) -> Double { return __tinyswift_fabs(x) }
func fmod(_ x: Double, _ y: Double) -> Double { return __tinyswift_fmod(x, y) }

let pi: Double = 3.14159265358979323846
let e: Double = 2.71828182845904523536
