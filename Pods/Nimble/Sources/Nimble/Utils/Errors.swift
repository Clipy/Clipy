import Foundation

// Generic

internal func setFailureMessageForError<T: ErrorType>(
    failureMessage: FailureMessage,
    postfixMessageVerb: String = "throw",
    actualError: ErrorType?,
    error: T? = nil,
    errorType: T.Type? = nil,
    closure: ((T) -> Void)? = nil) {
    failureMessage.postfixMessage = "\(postfixMessageVerb) error"

    if let error = error {
        if let error = error as? CustomDebugStringConvertible {
            failureMessage.postfixMessage += " <\(error.debugDescription)>"
        } else {
            failureMessage.postfixMessage += " <\(error)>"
        }
    } else if errorType != nil || closure != nil {
        failureMessage.postfixMessage += " from type <\(T.self)>"
    }
    if let _ = closure {
        failureMessage.postfixMessage += " that satisfies block"
    }
    if error == nil && errorType == nil && closure == nil {
        failureMessage.postfixMessage = "\(postfixMessageVerb) any error"
    }

    if let actualError = actualError {
        failureMessage.actualValue = "<\(actualError)>"
    } else {
        failureMessage.actualValue = "no error"
    }
}

internal func errorMatchesExpectedError<T: ErrorType>(
    actualError: ErrorType,
    expectedError: T) -> Bool {
    return actualError._domain == expectedError._domain
        && actualError._code   == expectedError._code
}

internal func errorMatchesExpectedError<T: ErrorType where T: Equatable>(
    actualError: ErrorType,
    expectedError: T) -> Bool {
    if let actualError = actualError as? T {
        return actualError == expectedError
    }
    return false
}

internal func errorMatchesNonNilFieldsOrClosure<T: ErrorType>(
    actualError: ErrorType?,
    error: T? = nil,
    errorType: T.Type? = nil,
    closure: ((T) -> Void)? = nil) -> Bool {
    var matches = false

    if let actualError = actualError {
        matches = true

        if let error = error {
            if !errorMatchesExpectedError(actualError, expectedError: error) {
                matches = false
            }
        }
        if let actualError = actualError as? T {
            if let closure = closure {
                let assertions = gatherFailingExpectations {
                    closure(actualError as T)
                }
                let messages = assertions.map { $0.message }
                if messages.count > 0 {
                    matches = false
                }
            }
        } else if errorType != nil && closure != nil {
            // The closure expects another ErrorType as argument, so this
            // is _supposed_ to fail, so that it becomes more obvious.
            let assertions = gatherExpectations {
                expect(actualError is T).to(equal(true))
            }
            precondition(assertions.map { $0.message }.count > 0)
            matches = false
        }
    }

    return matches
}

// Non-generic

internal func setFailureMessageForError(
    failureMessage: FailureMessage,
    actualError: ErrorType?,
    closure: ((ErrorType) -> Void)?) {
    failureMessage.postfixMessage = "throw error"

    if let _ = closure {
        failureMessage.postfixMessage += " that satisfies block"
    } else {
        failureMessage.postfixMessage = "throw any error"
    }

    if let actualError = actualError {
        failureMessage.actualValue = "<\(actualError)>"
    } else {
        failureMessage.actualValue = "no error"
    }
}

internal func errorMatchesNonNilFieldsOrClosure(
    actualError: ErrorType?,
    closure: ((ErrorType) -> Void)?) -> Bool {
    var matches = false

    if let actualError = actualError {
        matches = true

        if let closure = closure {
            let assertions = gatherFailingExpectations {
                closure(actualError)
            }
            let messages = assertions.map { $0.message }
            if messages.count > 0 {
                matches = false
            }
        }
    }

    return matches
}
