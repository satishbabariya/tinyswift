@frozen
public struct Bool {
  public var _value: Builtin.Int1

  @_transparent
  public init() {
    self._value = Builtin.zeroInitializer()
  }

  @_transparent
  public init(_ v: Builtin.Int1) {
    self._value = v
  }

  @_transparent
  public init(_ value: Bool) {
    self._value = value._value
  }
}