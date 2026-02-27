// TinySwift Example: Access Control
// Demonstrates public, private, internal, fileprivate, and static modifiers.

// --- Public members (accessible everywhere) ---

public func publicGreet() -> String {
  return "Hello from public function"
}

// --- Private members (accessible only within the enclosing scope) ---

private func privateHelper() -> Int {
  return 42
}

// --- Internal (default access, accessible within the module) ---

internal func internalFunction() -> Bool {
  return true
}

// --- Fileprivate (accessible within the same file) ---

fileprivate func filePrivateHelper() -> String {
  return "file-private"
}

// --- Access control on struct members ---

struct SecureConfig {
  private var secretKey: String
  private var token: String
  public var name: String

  init(name: String, key: String, token: String) {
    self.name = name
    self.secretKey = key
    self.token = token
  }

  public func getName() -> String {
    return name
  }

  private func getSecret() -> String {
    return secretKey
  }

  public func isValid() -> Bool {
    return secretKey != "" && token != ""
  }
}

// --- Access control on class members ---

class DatabaseConnection {
  private var connectionString: String
  private var isConnected: Bool

  init(connectionString: String) {
    self.connectionString = connectionString
    self.isConnected = false
  }

  public func connect() -> Bool {
    self.isConnected = true
    return true
  }

  public func disconnect() -> Void {
    self.isConnected = false
  }

  public func getStatus() -> Bool {
    return isConnected
  }

  private func executeRaw(_ query: String) -> String {
    return "result"
  }
}

// --- Static members ---

struct MathConstants {
  static let pi: Double = 3.14159265358979
  static let e: Double = 2.71828182845904
  static let tau: Double = 6.28318530717958
}

struct AppConfig {
  static var debugMode: Bool = false
  static var version: String = "1.0.0"

  static func enableDebug() -> Void {
    debugMode = true
  }

  static func disableDebug() -> Void {
    debugMode = false
  }
}

// --- Mixed access levels ---

class Logger {
  private var entries: [String]
  public var level: Int

  init(level: Int) {
    self.entries = []
    self.level = level
  }

  public func log(_ message: String) -> Void {
    // Add to entries
  }

  public func getEntryCount() -> Int {
    return 0
  }

  private func format(_ message: String) -> String {
    return message
  }

  fileprivate func flush() -> Void {
    self.entries = []
  }
}
