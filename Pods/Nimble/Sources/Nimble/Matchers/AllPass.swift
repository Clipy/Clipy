import Foundation

public func allPass<T, U>
    (_ passFunc: @escaping (T?) throws -> Bool) -> Predicate<U>
    where U: Sequence, T == U.Iterator.Element {
        let matcher = Predicate.simpleNilable("pass a condition") { actualExpression in
            return PredicateStatus(bool: try passFunc(try actualExpression.evaluate()))
        }
        return createPredicate(matcher)
}

public func allPass<T, U>
    (_ passName: String, _ passFunc: @escaping (T?) throws -> Bool) -> Predicate<U>
    where U: Sequence, T == U.Iterator.Element {
        let matcher = Predicate.simpleNilable(passName) { actualExpression in
            return PredicateStatus(bool: try passFunc(try actualExpression.evaluate()))
        }
        return createPredicate(matcher)
}

public func allPass<S, M>(_ elementMatcher: M) -> Predicate<S>
    where S: Sequence, M: Matcher, S.Iterator.Element == M.ValueType {
        return createPredicate(elementMatcher.predicate)
}

public func allPass<S>(_ elementPredicate: Predicate<S.Iterator.Element>) -> Predicate<S>
    where S: Sequence {
        return createPredicate(elementPredicate)
}

private func createPredicate<S>(_ elementMatcher: Predicate<S.Iterator.Element>) -> Predicate<S>
    where S: Sequence {
        return Predicate { actualExpression in
            guard let actualValue = try actualExpression.evaluate() else {
                return PredicateResult(
                    status: .fail,
                    message: .appends(.expectedTo("all pass"), " (use beNil() to match nils)")
                )
            }

            var failure: ExpectationMessage = .expectedTo("all pass")
            for currentElement in actualValue {
                let exp = Expression(
                    expression: {currentElement}, location: actualExpression.location)
                let predicateResult = try elementMatcher.satisfies(exp)
                if predicateResult.status == .matches {
                    failure = predicateResult.message.prepended(expectation: "all ")
                } else {
                    failure = predicateResult.message
                        .replacedExpectation({ .expectedTo($0.expectedMessage) })
                        .wrappedExpectation(
                            before: "all ",
                            after: ", but failed first at element <\(stringify(currentElement))>"
                                + " in <\(stringify(actualValue))>"
                    )
                    return PredicateResult(status: .doesNotMatch, message: failure)
                }
            }
            failure = failure.replacedExpectation({ expectation in
                return .expectedTo(expectation.expectedMessage)
            })
            return PredicateResult(status: .matches, message: failure)
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
            let pred: Predicate<[NSObject]> = createPredicate(Predicate.fromDeprecatedFullClosure { expr, failureMessage, expectMatch in
                if expectMatch {
                    return matcher.matches({ try! expr.evaluate() }, failureMessage: failureMessage, location: expr.location)
                } else {
                    return matcher.doesNotMatch({ try! expr.evaluate() }, failureMessage: failureMessage, location: expr.location)
                }
            })
            return try! pred.matches(expr, failureMessage: failureMessage)
        }
    }
}
#endif
