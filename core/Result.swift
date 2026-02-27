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

  func get() -> Success {
    switch self {
    case .success(let v): return v
    case .failure: fatalError("Result.get() called on failure")
    }
  }

  func map<NewSuccess>(_ transform: (Success) -> NewSuccess) -> Result<NewSuccess, Failure> {
    switch self {
    case .success(let v): return .success(transform(v))
    case .failure(let e): return .failure(e)
    }
  }

  func flatMap<NewSuccess>(_ transform: (Success) -> Result<NewSuccess, Failure>) -> Result<NewSuccess, Failure> {
    switch self {
    case .success(let v): return transform(v)
    case .failure(let e): return .failure(e)
    }
  }

  func mapFailure<NewFailure>(_ transform: (Failure) -> NewFailure) -> Result<Success, NewFailure> {
    switch self {
    case .success(let v): return .success(v)
    case .failure(let e): return .failure(transform(e))
    }
  }
}
