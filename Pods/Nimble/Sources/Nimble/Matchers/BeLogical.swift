import Foundation

extension Int8: ExpressibleByBooleanLiteral {
    public init(booleanLiteral value: Bool) {
        self = NSNumber(value: value).int8Value
    }
}

extension UInt8: ExpressibleByBooleanLiteral {
    public init(booleanLiteral value: Bool) {
        self = NSNumber(value: value).uint8Value
    }
}

extension Int16: ExpressibleByBooleanLiteral {
    public init(booleanLiteral value: Bool) {
        self = NSNumber(value: value).int16Value
    }
}

extension UInt16: ExpressibleByBooleanLiteral {
    public init(booleanLiteral value: Bool) {
        self = NSNumber(value: value).uint16Value
    }
}

extension Int32: ExpressibleByBooleanLiteral {
    public init(booleanLiteral value: Bool) {
        self = NSNumber(value: value).int32Value
    }
}

extension UInt32: ExpressibleByBooleanLiteral {
    public init(booleanLiteral value: Bool) {
        self = NSNumber(value: value).uint32Value
    }
}

extension Int64: ExpressibleByBooleanLiteral {
    public init(booleanLiteral value: Bool) {
        self = NSNumber(value: value).int64Value
    }
}

extension UInt64: ExpressibleByBooleanLiteral {
    public init(booleanLiteral value: Bool) {
        self = NSNumber(value: value).uint64Value
    }
}

extension Float: ExpressibleByBooleanLiteral {
    public init(booleanLiteral value: Bool) {
        self = NSNumber(value: value).floatValue
    }
}

extension Double: ExpressibleByBooleanLiteral {
    public init(booleanLiteral value: Bool) {
        self = NSNumber(value: value).doubleValue
    }
}

extension Int: ExpressibleByBooleanLiteral {
    public init(booleanLiteral value: Bool) {
        self = NSNumber(value: value).intValue
    }
}

extension UInt: ExpressibleByBooleanLiteral {
    public init(booleanLiteral value: Bool) {
        self = NSNumber(value: value).uintValue
    }
}

internal func matcherWithFailureMessage<T>(_ matcher: NonNilMatcherFunc<T>, postprocessor: @escaping (FailureMessage) -> Void) -> NonNilMatcherFunc<T> {
    return NonNilMatcherFunc { actualExpression, failureMessage in
        defer { postprocessor(failureMessage) }
        return try matcher.matcher(actualExpression, failureMessage)
    }
}

// MARK: beTrue() / beFalse()

/// A Nimble matcher that succeeds when the actual value is exactly true.
/// This matcher will not match against nils.
public func beTrue() -> NonNilMatcherFunc<Bool> {
    return matcherWithFailureMessage(equal(true)) { failureMessage in
        failureMessage.postfixMessage = "be true"
    }
}

/// A Nimble matcher that succeeds when the actual value is exactly false.
/// This matcher will not match against nils.
public func beFalse() -> NonNilMatcherFunc<Bool> {
    return matcherWithFailureMessage(equal(false)) { failureMessage in
        failureMessage.postfixMessage = "be false"
    }
}

// MARK: beTruthy() / beFalsy()

/// A Nimble matcher that succeeds when the actual value is not logically false.
public func beTruthy<T: ExpressibleByBooleanLiteral & Equatable>() -> MatcherFunc<T> {
    return MatcherFunc { actualExpression, failureMessage in
        failureMessage.postfixMessage = "be truthy"
        let actualValue = try actualExpression.evaluate()
        if let actualValue = actualValue {
            // FIXME: This is a workaround to SR-2290.
            // See:
            // - https://bugs.swift.org/browse/SR-2290
            // - https://github.com/norio-nomura/Nimble/pull/5#issuecomment-237835873
            if let number = actualValue as? NSNumber {
                return number.boolValue == true
            }

            return actualValue == (true as T)
        }
        return actualValue != nil
    }
}

/// A Nimble matcher that succeeds when the actual value is logically false.
/// This matcher will match against nils.
public func beFalsy<T: ExpressibleByBooleanLiteral & Equatable>() -> MatcherFunc<T> {
    return MatcherFunc { actualExpression, failureMessage in
        failureMessage.postfixMessage = "be falsy"
        let actualValue = try actualExpression.evaluate()
        if let actualValue = actualValue {
            // FIXME: This is a workaround to SR-2290.
            // See:
            // - https://bugs.swift.org/browse/SR-2290
            // - https://github.com/norio-nomura/Nimble/pull/5#issuecomment-237835873
            if let number = actualValue as? NSNumber {
                return number.boolValue == false
            }

            return actualValue == (false as T)
        }
        return actualValue == nil
    }
}

#if _runtime(_ObjC)
extension NMBObjCMatcher {
    public class func beTruthyMatcher() -> NMBObjCMatcher {
        return NMBObjCMatcher { actualExpression, failureMessage in
            let expr = actualExpression.cast { ($0 as? NSNumber)?.boolValue ?? false }
            return try! beTruthy().matches(expr, failureMessage: failureMessage)
        }
    }

    public class func beFalsyMatcher() -> NMBObjCMatcher {
        return NMBObjCMatcher { actualExpression, failureMessage in
            let expr = actualExpression.cast { ($0 as? NSNumber)?.boolValue ?? false }
            return try! beFalsy().matches(expr, failureMessage: failureMessage)
        }
    }

    public class func beTrueMatcher() -> NMBObjCMatcher {
        return NMBObjCMatcher { actualExpression, failureMessage in
            let expr = actualExpression.cast { ($0 as? NSNumber)?.boolValue ?? false }
            return try! beTrue().matches(expr, failureMessage: failureMessage)
        }
    }

    public class func beFalseMatcher() -> NMBObjCMatcher {
        return NMBObjCMatcher(canMatchNil: false) { actualExpression, failureMessage in
            let expr = actualExpression.cast { ($0 as? NSNumber)?.boolValue ?? false }
            return try! beFalse().matches(expr, failureMessage: failureMessage)
        }
    }
}
#endif
