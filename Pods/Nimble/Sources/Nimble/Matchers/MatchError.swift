import Foundation

/// A Nimble matcher that succeeds when the actual expression evaluates to an
/// error from the specified case.
///
/// Errors are tried to be compared by their implementation of Equatable,
/// otherwise they fallback to comparision by _domain and _code.
public func matchError<T: ErrorType>(error: T) -> NonNilMatcherFunc<ErrorType> {
    return NonNilMatcherFunc { actualExpression, failureMessage in
        let actualError: ErrorType? = try actualExpression.evaluate()

        setFailureMessageForError(failureMessage, postfixMessageVerb: "match", actualError: actualError, error: error)
        return errorMatchesNonNilFieldsOrClosure(actualError, error: error)
    }
}

/// A Nimble matcher that succeeds when the actual expression evaluates to an
/// error of the specified type
public func matchError<T: ErrorType>(errorType: T.Type) -> NonNilMatcherFunc<ErrorType> {
    return NonNilMatcherFunc { actualExpression, failureMessage in
        let actualError: ErrorType? = try actualExpression.evaluate()

        setFailureMessageForError(failureMessage, postfixMessageVerb: "match", actualError: actualError, errorType: errorType)
        return errorMatchesNonNilFieldsOrClosure(actualError, errorType: errorType)
    }
}
