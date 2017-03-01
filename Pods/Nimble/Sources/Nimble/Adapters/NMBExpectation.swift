import Foundation

#if _runtime(_ObjC)

internal struct ObjCMatcherWrapper: Matcher {
    let matcher: NMBMatcher

    func matches(_ actualExpression: Expression<NSObject>, failureMessage: FailureMessage) -> Bool {
        return matcher.matches(
            ({ try! actualExpression.evaluate() }),
            failureMessage: failureMessage,
            location: actualExpression.location)
    }

    func doesNotMatch(_ actualExpression: Expression<NSObject>, failureMessage: FailureMessage) -> Bool {
        return matcher.doesNotMatch(
            ({ try! actualExpression.evaluate() }),
            failureMessage: failureMessage,
            location: actualExpression.location)
    }
}

// Equivalent to Expectation, but for Nimble's Objective-C interface
public class NMBExpectation: NSObject {
    internal let _actualBlock: () -> NSObject!
    internal var _negative: Bool
    internal let _file: FileString
    internal let _line: UInt
    internal var _timeout: TimeInterval = 1.0

    public init(actualBlock: @escaping () -> NSObject!, negative: Bool, file: FileString, line: UInt) {
        self._actualBlock = actualBlock
        self._negative = negative
        self._file = file
        self._line = line
    }

    private var expectValue: Expectation<NSObject> {
        return expect(_file, line: _line) {
            self._actualBlock() as NSObject?
        }
    }

    public var withTimeout: (TimeInterval) -> NMBExpectation {
        return ({ timeout in self._timeout = timeout
            return self
        })
    }

    public var to: (NMBMatcher) -> Void {
        return ({ matcher in
            self.expectValue.to(ObjCMatcherWrapper(matcher: matcher))
        })
    }

    public var toWithDescription: (NMBMatcher, String) -> Void {
        return ({ matcher, description in
            self.expectValue.to(ObjCMatcherWrapper(matcher: matcher), description: description)
        })
    }

    public var toNot: (NMBMatcher) -> Void {
        return ({ matcher in
            self.expectValue.toNot(
                ObjCMatcherWrapper(matcher: matcher)
            )
        })
    }

    public var toNotWithDescription: (NMBMatcher, String) -> Void {
        return ({ matcher, description in
            self.expectValue.toNot(
                ObjCMatcherWrapper(matcher: matcher), description: description
            )
        })
    }

    public var notTo: (NMBMatcher) -> Void { return toNot }

    public var notToWithDescription: (NMBMatcher, String) -> Void { return toNotWithDescription }

    public var toEventually: (NMBMatcher) -> Void {
        return ({ matcher in
            self.expectValue.toEventually(
                ObjCMatcherWrapper(matcher: matcher),
                timeout: self._timeout,
                description: nil
            )
        })
    }

    public var toEventuallyWithDescription: (NMBMatcher, String) -> Void {
        return ({ matcher, description in
            self.expectValue.toEventually(
                ObjCMatcherWrapper(matcher: matcher),
                timeout: self._timeout,
                description: description
            )
        })
    }

    public var toEventuallyNot: (NMBMatcher) -> Void {
        return ({ matcher in
            self.expectValue.toEventuallyNot(
                ObjCMatcherWrapper(matcher: matcher),
                timeout: self._timeout,
                description: nil
            )
        })
    }

    public var toEventuallyNotWithDescription: (NMBMatcher, String) -> Void {
        return ({ matcher, description in
            self.expectValue.toEventuallyNot(
                ObjCMatcherWrapper(matcher: matcher),
                timeout: self._timeout,
                description: description
            )
        })
    }

    public var toNotEventually: (NMBMatcher) -> Void { return toEventuallyNot }

    public var toNotEventuallyWithDescription: (NMBMatcher, String) -> Void { return toEventuallyNotWithDescription }

    public class func failWithMessage(_ message: String, file: FileString, line: UInt) {
        fail(message, location: SourceLocation(file: file, line: line))
    }
}

#endif
