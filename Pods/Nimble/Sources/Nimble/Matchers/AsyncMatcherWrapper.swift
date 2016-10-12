import Foundation

#if _runtime(_ObjC)

public struct AsyncDefaults {
    public static var Timeout: NSTimeInterval = 1
    public static var PollInterval: NSTimeInterval = 0.01
}

internal struct AsyncMatcherWrapper<T, U where U: Matcher, U.ValueType == T>: Matcher {
    let fullMatcher: U
    let timeoutInterval: NSTimeInterval
    let pollInterval: NSTimeInterval

    init(fullMatcher: U, timeoutInterval: NSTimeInterval = AsyncDefaults.Timeout, pollInterval: NSTimeInterval = AsyncDefaults.PollInterval) {
      self.fullMatcher = fullMatcher
      self.timeoutInterval = timeoutInterval
      self.pollInterval = pollInterval
    }

    func matches(actualExpression: Expression<T>, failureMessage: FailureMessage) -> Bool {
        let uncachedExpression = actualExpression.withoutCaching()
        let fnName = "expect(...).toEventually(...)"
        let result = pollBlock(
            pollInterval: pollInterval,
            timeoutInterval: timeoutInterval,
            file: actualExpression.location.file,
            line: actualExpression.location.line,
            fnName: fnName) {
                try self.fullMatcher.matches(uncachedExpression, failureMessage: failureMessage)
        }
        switch (result) {
        case let .Completed(isSuccessful): return isSuccessful
        case .TimedOut: return false
        case let .ErrorThrown(error):
            failureMessage.actualValue = "an unexpected error thrown: <\(error)>"
            return false
        case let .RaisedException(exception):
            failureMessage.actualValue = "an unexpected exception thrown: <\(exception)>"
            return false
        case .BlockedRunLoop:
            failureMessage.postfixMessage += " (timed out, but main thread was unresponsive)."
            return false
        case .Incomplete:
            internalError("Reached .Incomplete state for toEventually(...).")
        }
    }

    func doesNotMatch(actualExpression: Expression<T>, failureMessage: FailureMessage) -> Bool  {
        let uncachedExpression = actualExpression.withoutCaching()
        let result = pollBlock(
            pollInterval: pollInterval,
            timeoutInterval: timeoutInterval,
            file: actualExpression.location.file,
            line: actualExpression.location.line,
            fnName: "expect(...).toEventuallyNot(...)") {
                try self.fullMatcher.doesNotMatch(uncachedExpression, failureMessage: failureMessage)
        }
        switch (result) {
        case let .Completed(isSuccessful): return isSuccessful
        case .TimedOut: return false
        case let .ErrorThrown(error):
            failureMessage.actualValue = "an unexpected error thrown: <\(error)>"
            return false
        case let .RaisedException(exception):
            failureMessage.actualValue = "an unexpected exception thrown: <\(exception)>"
            return false
        case .BlockedRunLoop:
            failureMessage.postfixMessage += " (timed out, but main thread was unresponsive)."
            return false
        case .Incomplete:
            internalError("Reached .Incomplete state for toEventuallyNot(...).")
        }
    }
}

private let toEventuallyRequiresClosureError = FailureMessage(stringValue: "expect(...).toEventually(...) requires an explicit closure (eg - expect { ... }.toEventually(...) )\nSwift 1.2 @autoclosure behavior has changed in an incompatible way for Nimble to function")


extension Expectation {
    /// Tests the actual value using a matcher to match by checking continuously
    /// at each pollInterval until the timeout is reached.
    ///
    /// @discussion
    /// This function manages the main run loop (`NSRunLoop.mainRunLoop()`) while this function
    /// is executing. Any attempts to touch the run loop may cause non-deterministic behavior.
    public func toEventually<U where U: Matcher, U.ValueType == T>(matcher: U, timeout: NSTimeInterval = AsyncDefaults.Timeout, pollInterval: NSTimeInterval = AsyncDefaults.PollInterval, description: String? = nil) {
        if expression.isClosure {
            let (pass, msg) = expressionMatches(
                expression,
                matcher: AsyncMatcherWrapper(
                    fullMatcher: matcher,
                    timeoutInterval: timeout,
                    pollInterval: pollInterval),
                to: "to eventually",
                description: description
            )
            verify(pass, msg)
        } else {
            verify(false, toEventuallyRequiresClosureError)
        }
    }

    /// Tests the actual value using a matcher to not match by checking
    /// continuously at each pollInterval until the timeout is reached.
    ///
    /// @discussion
    /// This function manages the main run loop (`NSRunLoop.mainRunLoop()`) while this function
    /// is executing. Any attempts to touch the run loop may cause non-deterministic behavior.
    public func toEventuallyNot<U where U: Matcher, U.ValueType == T>(matcher: U, timeout: NSTimeInterval = AsyncDefaults.Timeout, pollInterval: NSTimeInterval = AsyncDefaults.PollInterval, description: String? = nil) {
        if expression.isClosure {
            let (pass, msg) = expressionDoesNotMatch(
                expression,
                matcher: AsyncMatcherWrapper(
                    fullMatcher: matcher,
                    timeoutInterval: timeout,
                    pollInterval: pollInterval),
                toNot: "to eventually not",
                description: description
            )
            verify(pass, msg)
        } else {
            verify(false, toEventuallyRequiresClosureError)
        }
    }

    /// Tests the actual value using a matcher to not match by checking
    /// continuously at each pollInterval until the timeout is reached.
    ///
    /// Alias of toEventuallyNot()
    ///
    /// @discussion
    /// This function manages the main run loop (`NSRunLoop.mainRunLoop()`) while this function
    /// is executing. Any attempts to touch the run loop may cause non-deterministic behavior.
    public func toNotEventually<U where U: Matcher, U.ValueType == T>(matcher: U, timeout: NSTimeInterval = AsyncDefaults.Timeout, pollInterval: NSTimeInterval = AsyncDefaults.PollInterval, description: String? = nil) {
        return toEventuallyNot(matcher, timeout: timeout, pollInterval: pollInterval, description: description)
    }
}

#endif
