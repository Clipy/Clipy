import Foundation

/// A Nimble matcher that succeeds when the actual expression throws an
/// error of the specified type or from the specified case.
///
/// Errors are tried to be compared by their implementation of Equatable,
/// otherwise they fallback to comparision by _domain and _code.
///
/// Alternatively, you can pass a closure to do any arbitrary custom matching
/// to the thrown error. The closure only gets called when an error was thrown.
///
/// nil arguments indicates that the matcher should not attempt to match against
/// that parameter.
public func throwError<T: Error>(
    _ error: T? = nil,
    errorType: T.Type? = nil,
    closure: ((T) -> Void)? = nil) -> MatcherFunc<Any> {
        return MatcherFunc { actualExpression, failureMessage in

            var actualError: Error?
            do {
                _ = try actualExpression.evaluate()
            } catch let catchedError {
                actualError = catchedError
            }

            setFailureMessageForError(failureMessage, actualError: actualError, error: error, errorType: errorType, closure: closure)
            return errorMatchesNonNilFieldsOrClosure(actualError, error: error, errorType: errorType, closure: closure)
        }
}

/// A Nimble matcher that succeeds when the actual expression throws any
/// error or when the passed closures' arbitrary custom matching succeeds.
///
/// This duplication to it's generic adequate is required to allow to receive
/// values of the existential type `Error` in the closure.
///
/// The closure only gets called when an error was thrown.
public func throwError(
    closure: ((Error) -> Void)? = nil) -> MatcherFunc<Any> {
        return MatcherFunc { actualExpression, failureMessage in

            var actualError: Error?
            do {
                _ = try actualExpression.evaluate()
            } catch let catchedError {
                actualError = catchedError
            }

            setFailureMessageForError(failureMessage, actualError: actualError, closure: closure)
            return errorMatchesNonNilFieldsOrClosure(actualError, closure: closure)
        }
}
