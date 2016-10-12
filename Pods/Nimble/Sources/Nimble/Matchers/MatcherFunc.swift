/// A convenience API to build matchers that don't need special negation
/// behavior. The toNot() behavior is the negation of to().
///
/// @see NonNilMatcherFunc if you prefer to have this matcher fail when nil
///                        values are recieved in an expectation.
///
/// You may use this when implementing your own custom matchers.
///
/// Use the Matcher protocol instead of this type to accept custom matchers as
/// input parameters.
/// @see allPass for an example that uses accepts other matchers as input.
public struct MatcherFunc<T>: Matcher {
    public let matcher: (Expression<T>, FailureMessage) throws -> Bool

    public init(_ matcher: (Expression<T>, FailureMessage) throws -> Bool) {
        self.matcher = matcher
    }

    public func matches(actualExpression: Expression<T>, failureMessage: FailureMessage) throws -> Bool {
        return try matcher(actualExpression, failureMessage)
    }

    public func doesNotMatch(actualExpression: Expression<T>, failureMessage: FailureMessage) throws -> Bool {
        return try !matcher(actualExpression, failureMessage)
    }
}

/// A convenience API to build matchers that don't need special negation
/// behavior. The toNot() behavior is the negation of to().
///
/// Unlike MatcherFunc, this will always fail if an expectation contains nil.
/// This applies regardless of using to() or toNot().
///
/// You may use this when implementing your own custom matchers.
///
/// Use the Matcher protocol instead of this type to accept custom matchers as
/// input parameters.
/// @see allPass for an example that uses accepts other matchers as input.
public struct NonNilMatcherFunc<T>: Matcher {
    public let matcher: (Expression<T>, FailureMessage) throws -> Bool

    public init(_ matcher: (Expression<T>, FailureMessage) throws -> Bool) {
        self.matcher = matcher
    }

    public func matches(actualExpression: Expression<T>, failureMessage: FailureMessage) throws -> Bool {
        let pass = try matcher(actualExpression, failureMessage)
        if try attachNilErrorIfNeeded(actualExpression, failureMessage: failureMessage) {
            return false
        }
        return pass
    }

    public func doesNotMatch(actualExpression: Expression<T>, failureMessage: FailureMessage) throws -> Bool {
        let pass = try !matcher(actualExpression, failureMessage)
        if try attachNilErrorIfNeeded(actualExpression, failureMessage: failureMessage) {
            return false
        }
        return pass
    }

    internal func attachNilErrorIfNeeded(actualExpression: Expression<T>, failureMessage: FailureMessage) throws -> Bool {
        if try actualExpression.evaluate() == nil {
            failureMessage.postfixActual = " (use beNil() to match nils)"
            return true
        }
        return false
    }
}
