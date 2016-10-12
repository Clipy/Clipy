import Foundation

// A Nimble matcher that catches attempts to use beAnInstanceOf with non Objective-C types
public func beAnInstanceOf(expectedClass: Any) -> NonNilMatcherFunc<Any> {
    return NonNilMatcherFunc {actualExpression, failureMessage in
        failureMessage.stringValue = "beAnInstanceOf only works on Objective-C types since"
            + " the Swift compiler will automatically type check Swift-only types."
            + " This expectation is redundant."
        return false
    }
}

/// A Nimble matcher that succeeds when the actual value is an instance of the given class.
/// @see beAKindOf if you want to match against subclasses
public func beAnInstanceOf(expectedClass: AnyClass) -> NonNilMatcherFunc<NSObject> {
    return NonNilMatcherFunc { actualExpression, failureMessage in
        let instance = try actualExpression.evaluate()
        if let validInstance = instance {
            failureMessage.actualValue = "<\(classAsString(validInstance.dynamicType)) instance>"
        } else {
            failureMessage.actualValue = "<nil>"
        }
        failureMessage.postfixMessage = "be an instance of \(classAsString(expectedClass))"
#if _runtime(_ObjC)
        return instance != nil && instance!.isMemberOfClass(expectedClass)
#else
        return instance != nil && instance!.dynamicType == expectedClass
#endif
    }
}

#if _runtime(_ObjC)
extension NMBObjCMatcher {
    public class func beAnInstanceOfMatcher(expected: AnyClass) -> NMBMatcher {
        return NMBObjCMatcher(canMatchNil: false) { actualExpression, failureMessage in
            return try! beAnInstanceOf(expected).matches(actualExpression, failureMessage: failureMessage)
        }
    }
}
#endif
