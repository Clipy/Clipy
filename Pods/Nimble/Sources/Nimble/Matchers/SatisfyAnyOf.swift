import Foundation

/// A Nimble matcher that succeeds when the actual value matches with any of the matchers
/// provided in the variable list of matchers. 
public func satisfyAnyOf<T,U>(_ matchers: U...) -> NonNilMatcherFunc<T>
    where U: Matcher, U.ValueType == T
{
    return satisfyAnyOf(matchers)
}

internal func satisfyAnyOf<T,U>(_ matchers: [U]) -> NonNilMatcherFunc<T>
    where U: Matcher, U.ValueType == T
{
    return NonNilMatcherFunc<T> { actualExpression, failureMessage in
        let postfixMessages = NSMutableArray()
        var matches = false
        for matcher in matchers {
            if try matcher.matches(actualExpression, failureMessage: failureMessage) {
                matches = true
            }
            postfixMessages.add(NSString(string: "{\(failureMessage.postfixMessage)}"))
        }

        failureMessage.postfixMessage = "match one of: " + postfixMessages.componentsJoined(by: ", or ")
        if let actualValue = try actualExpression.evaluate() {
            failureMessage.actualValue = "\(actualValue)"
        }

        return matches
    }
}

public func ||<T>(left: NonNilMatcherFunc<T>, right: NonNilMatcherFunc<T>) -> NonNilMatcherFunc<T> {
    return satisfyAnyOf(left, right)
}

public func ||<T>(left: MatcherFunc<T>, right: MatcherFunc<T>) -> NonNilMatcherFunc<T> {
    return satisfyAnyOf(left, right)
}

#if _runtime(_ObjC)
extension NMBObjCMatcher {
    public class func satisfyAnyOfMatcher(_ matchers: [NMBObjCMatcher]) -> NMBObjCMatcher {
        return NMBObjCMatcher(canMatchNil: false) { actualExpression, failureMessage in
            if matchers.isEmpty {
                failureMessage.stringValue = "satisfyAnyOf must be called with at least one matcher"
                return false
            }
            
            var elementEvaluators = [NonNilMatcherFunc<NSObject>]()
            for matcher in matchers {
                let elementEvaluator: (Expression<NSObject>, FailureMessage) -> Bool = {
                    expression, failureMessage in
                    return matcher.matches(
                        {try! expression.evaluate()}, failureMessage: failureMessage, location: actualExpression.location)
                }
                
                elementEvaluators.append(NonNilMatcherFunc(elementEvaluator))
            }
            
            return try! satisfyAnyOf(elementEvaluators).matches(actualExpression, failureMessage: failureMessage)
        }
    }
}
#endif
