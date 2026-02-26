// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// M89: Result<Success, Failure> — a generic enum for representing either
// a success value or a failure value.

enum Result<Success, Failure> {
  case success(Success)
  case failure(Failure)

  func isSuccess() -> Bool {
    switch self {
    case .success: return true
    case .failure: return false
    }
  }

  func isFailure() -> Bool {
    switch self {
    case .success: return false
    case .failure: return true
    }
  }

  func getSuccess() -> Success? {
    switch self {
    case .success(let v): return v
    case .failure: return nil
    }
  }

  func getFailure() -> Failure? {
    switch self {
    case .success: return nil
    case .failure(let e): return e
    }
  }
}
