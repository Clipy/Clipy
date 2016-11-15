import Foundation


/// A Nimble matcher that succeeds when the actual sequence's first element
/// is equal to the expected value.
public func beginWith<S: Sequence, T: Equatable>(_ startingElement: T) -> NonNilMatcherFunc<S>
    where S.Iterator.Element == T
{
    return NonNilMatcherFunc { actualExpression, failureMessage in
        failureMessage.postfixMessage = "begin with <\(startingElement)>"
        if let actualValue = try actualExpression.evaluate() {
            var actualGenerator = actualValue.makeIterator()
            return actualGenerator.next() == startingElement
        }
        return false
    }
}

/// A Nimble matcher that succeeds when the actual collection's first element
/// is equal to the expected object.
public func beginWith(_ startingElement: Any) -> NonNilMatcherFunc<NMBOrderedCollection> {
    return NonNilMatcherFunc { actualExpression, failureMessage in
        failureMessage.postfixMessage = "begin with <\(startingElement)>"
        guard let collection = try actualExpression.evaluate() else { return false }
        guard collection.count > 0 else { return false }
        #if os(Linux)
            guard let collectionValue = collection.object(at: 0) as? NSObject else {
                return false
            }
        #else
            let collectionValue = collection.object(at: 0) as AnyObject
        #endif
        return collectionValue.isEqual(startingElement)
    }
}

/// A Nimble matcher that succeeds when the actual string contains expected substring
/// where the expected substring's location is zero.
public func beginWith(_ startingSubstring: String) -> NonNilMatcherFunc<String> {
    return NonNilMatcherFunc { actualExpression, failureMessage in
        failureMessage.postfixMessage = "begin with <\(startingSubstring)>"
        if let actual = try actualExpression.evaluate() {
            let range = actual.range(of: startingSubstring)
            return range != nil && range!.lowerBound == actual.startIndex
        }
        return false
    }
}

#if _runtime(_ObjC)
extension NMBObjCMatcher {
    public class func beginWithMatcher(_ expected: Any) -> NMBObjCMatcher {
        return NMBObjCMatcher(canMatchNil: false) { actualExpression, failureMessage in
            let actual = try! actualExpression.evaluate()
            if let _ = actual as? String {
                let expr = actualExpression.cast { $0 as? String }
                return try! beginWith(expected as! String).matches(expr, failureMessage: failureMessage)
            } else {
                let expr = actualExpression.cast { $0 as? NMBOrderedCollection }
                return try! beginWith(expected).matches(expr, failureMessage: failureMessage)
            }
        }
    }
}
#endif
