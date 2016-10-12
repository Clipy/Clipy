import Foundation


/// A Nimble matcher that succeeds when the actual value is the same instance
/// as the expected instance.
public func beIdenticalTo(expected: AnyObject?) -> NonNilMatcherFunc<AnyObject> {
    return NonNilMatcherFunc { actualExpression, failureMessage in
        let actual = try actualExpression.evaluate()
        failureMessage.actualValue = "\(identityAsString(actual))"
        failureMessage.postfixMessage = "be identical to \(identityAsString(expected))"
        return actual === expected && actual !== nil
    }
}

public func ===(lhs: Expectation<AnyObject>, rhs: AnyObject?) {
    lhs.to(beIdenticalTo(rhs))
}
public func !==(lhs: Expectation<AnyObject>, rhs: AnyObject?) {
    lhs.toNot(beIdenticalTo(rhs))
}

/// A Nimble matcher that succeeds when the actual value is the same instance
/// as the expected instance.
///
/// Alias for "beIdenticalTo".
public func be(expected: AnyObject?) -> NonNilMatcherFunc<AnyObject> {
    return beIdenticalTo(expected)
}

#if _runtime(_ObjC)
extension NMBObjCMatcher {
    public class func beIdenticalToMatcher(expected: NSObject?) -> NMBObjCMatcher {
        return NMBObjCMatcher(canMatchNil: false) { actualExpression, failureMessage in
            let aExpr = actualExpression.cast { $0 as AnyObject? }
            return try! beIdenticalTo(expected).matches(aExpr, failureMessage: failureMessage)
        }
    }
}
#endif
