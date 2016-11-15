import Foundation

public func allPass<T,U>
    (_ passFunc: @escaping (T?) -> Bool) -> NonNilMatcherFunc<U>
    where U: Sequence, U.Iterator.Element == T
{
    return allPass("pass a condition", passFunc)
}

public func allPass<T,U>
    (_ passName: String, _ passFunc: @escaping (T?) -> Bool) -> NonNilMatcherFunc<U>
    where U: Sequence, U.Iterator.Element == T
{
    return createAllPassMatcher() {
        expression, failureMessage in
        failureMessage.postfixMessage = passName
        return passFunc(try expression.evaluate())
    }
}

public func allPass<U,V>
    (_ matcher: V) -> NonNilMatcherFunc<U>
    where U: Sequence, V: Matcher, U.Iterator.Element == V.ValueType
{
    return createAllPassMatcher() {
        try matcher.matches($0, failureMessage: $1)
    }
}

private func createAllPassMatcher<T,U>
    (_ elementEvaluator: @escaping (Expression<T>, FailureMessage) throws -> Bool) -> NonNilMatcherFunc<U>
    where U: Sequence, U.Iterator.Element == T
{
    return NonNilMatcherFunc { actualExpression, failureMessage in
        failureMessage.actualValue = nil
        if let actualValue = try actualExpression.evaluate() {
            for currentElement in actualValue {
                let exp = Expression(
                    expression: {currentElement}, location: actualExpression.location)
                if try !elementEvaluator(exp, failureMessage) {
                    failureMessage.postfixMessage =
                        "all \(failureMessage.postfixMessage),"
                        + " but failed first at element <\(stringify(currentElement))>"
                        + " in <\(stringify(actualValue))>"
                    return false
                }
            }
            failureMessage.postfixMessage = "all \(failureMessage.postfixMessage)"
        } else {
            failureMessage.postfixMessage = "all pass (use beNil() to match nils)"
            return false
        }

        return true
    }
}

#if _runtime(_ObjC)
extension NMBObjCMatcher {
    public class func allPassMatcher(_ matcher: NMBObjCMatcher) -> NMBObjCMatcher {
        return NMBObjCMatcher(canMatchNil: false) { actualExpression, failureMessage in
            let location = actualExpression.location
            let actualValue = try! actualExpression.evaluate()
            var nsObjects = [NSObject]()
            
            var collectionIsUsable = true
            if let value = actualValue as? NSFastEnumeration {
                let generator = NSFastEnumerationIterator(value)
                while let obj = generator.next() {
                    if let nsObject = obj as? NSObject {
                        nsObjects.append(nsObject)
                    } else {
                        collectionIsUsable = false
                        break
                    }
                }
            } else {
                collectionIsUsable = false
            }
            
            if !collectionIsUsable {
                failureMessage.postfixMessage =
                  "allPass only works with NSFastEnumeration (NSArray, NSSet, ...) of NSObjects"
                failureMessage.expected = ""
                failureMessage.to = ""
                return false
            }
            
            let expr = Expression(expression: ({ nsObjects }), location: location)
            let elementEvaluator: (Expression<NSObject>, FailureMessage) -> Bool = {
                expression, failureMessage in
                return matcher.matches(
                    {try! expression.evaluate()}, failureMessage: failureMessage, location: expr.location)
            }
            return try! createAllPassMatcher(elementEvaluator).matches(
                expr, failureMessage: failureMessage)
        }
    }
}
#endif
