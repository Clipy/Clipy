import Foundation


/// A Nimble matcher that succeeds when the actual sequence's last element
/// is equal to the expected value.
public func endWith<S: Sequence, T: Equatable>(_ endingElement: T) -> NonNilMatcherFunc<S>
    where S.Iterator.Element == T
{
    return NonNilMatcherFunc { actualExpression, failureMessage in
        failureMessage.postfixMessage = "end with <\(endingElement)>"

        if let actualValue = try actualExpression.evaluate() {
            var actualGenerator = actualValue.makeIterator()
            var lastItem: T?
            var item: T?
            repeat {
                lastItem = item
                item = actualGenerator.next()
            } while(item != nil)
            
            return lastItem == endingElement
        }
        return false
    }
}

/// A Nimble matcher that succeeds when the actual collection's last element
/// is equal to the expected object.
public func endWith(_ endingElement: Any) -> NonNilMatcherFunc<NMBOrderedCollection> {
    return NonNilMatcherFunc { actualExpression, failureMessage in
        failureMessage.postfixMessage = "end with <\(endingElement)>"
        guard let collection = try actualExpression.evaluate() else { return false }
        guard collection.count > 0 else { return false }
        #if os(Linux)
            guard let collectionValue = collection.object(at: collection.count - 1) as? NSObject else {
                return false
            }
        #else
            let collectionValue = collection.object(at: collection.count - 1) as AnyObject
        #endif

        return collectionValue.isEqual(endingElement)
    }
}


/// A Nimble matcher that succeeds when the actual string contains the expected substring
/// where the expected substring's location is the actual string's length minus the
/// expected substring's length.
public func endWith(_ endingSubstring: String) -> NonNilMatcherFunc<String> {
    return NonNilMatcherFunc { actualExpression, failureMessage in
        failureMessage.postfixMessage = "end with <\(endingSubstring)>"
        if let collection = try actualExpression.evaluate() {
            return collection.hasSuffix(endingSubstring)
        }
        return false
    }
}

#if _runtime(_ObjC)
extension NMBObjCMatcher {
    public class func endWithMatcher(_ expected: Any) -> NMBObjCMatcher {
        return NMBObjCMatcher(canMatchNil: false) { actualExpression, failureMessage in
            let actual = try! actualExpression.evaluate()
            if let _ = actual as? String {
                let expr = actualExpression.cast { $0 as? String }
                return try! endWith(expected as! String).matches(expr, failureMessage: failureMessage)
            } else {
                let expr = actualExpression.cast { $0 as? NMBOrderedCollection }
                return try! endWith(expected).matches(expr, failureMessage: failureMessage)
            }
        }
    }
}
#endif
