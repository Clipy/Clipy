import Foundation

/// A Nimble matcher that succeeds when the actual value is Void.
public func beVoid() -> MatcherFunc<()> {
    return MatcherFunc { actualExpression, failureMessage in
        failureMessage.postfixMessage = "be void"
        let actualValue: ()? = try actualExpression.evaluate()
        return actualValue != nil
    }
}

public func == (lhs: Expectation<()>, rhs: ()) {
    lhs.to(beVoid())
}

public func != (lhs: Expectation<()>, rhs: ()) {
    lhs.toNot(beVoid())
}
