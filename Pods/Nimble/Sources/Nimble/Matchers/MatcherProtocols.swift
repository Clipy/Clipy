import Foundation

/// Implement this protocol to implement a custom matcher for Swift
public protocol Matcher {
    associatedtype ValueType
    func matches(actualExpression: Expression<ValueType>, failureMessage: FailureMessage) throws -> Bool
    func doesNotMatch(actualExpression: Expression<ValueType>, failureMessage: FailureMessage) throws -> Bool
}

#if _runtime(_ObjC)
/// Objective-C interface to the Swift variant of Matcher.
@objc public protocol NMBMatcher {
    func matches(actualBlock: () -> NSObject!, failureMessage: FailureMessage, location: SourceLocation) -> Bool
    func doesNotMatch(actualBlock: () -> NSObject!, failureMessage: FailureMessage, location: SourceLocation) -> Bool
}
#endif

#if _runtime(_ObjC)
/// Protocol for types that support contain() matcher.
@objc public protocol NMBContainer {
    func containsObject(object: AnyObject!) -> Bool
}

extension NSHashTable : NMBContainer {} // Corelibs Foundation does not include this class yet
#else
public protocol NMBContainer {
    func containsObject(object: AnyObject) -> Bool
}
#endif

extension NSArray : NMBContainer {}
extension NSSet : NMBContainer {}

#if _runtime(_ObjC)
/// Protocol for types that support only beEmpty(), haveCount() matchers
@objc public protocol NMBCollection {
    var count: Int { get }
}

extension NSHashTable : NMBCollection {} // Corelibs Foundation does not include these classes yet
extension NSMapTable : NMBCollection {}
#else
public protocol NMBCollection {
    var count: Int { get }
}
#endif

extension NSSet : NMBCollection {}
extension NSIndexSet : NMBCollection {}
extension NSDictionary : NMBCollection {}

#if _runtime(_ObjC)
/// Protocol for types that support beginWith(), endWith(), beEmpty() matchers
@objc public protocol NMBOrderedCollection : NMBCollection {
    func indexOfObject(object: AnyObject!) -> Int
}
#else
public protocol NMBOrderedCollection : NMBCollection {
    func indexOfObject(object: AnyObject) -> Int
}
#endif

extension NSArray : NMBOrderedCollection {}

#if _runtime(_ObjC)
/// Protocol for types to support beCloseTo() matcher
@objc public protocol NMBDoubleConvertible {
    var doubleValue: CDouble { get }
}
#else
public protocol NMBDoubleConvertible {
    var doubleValue: CDouble { get }
}

extension Double : NMBDoubleConvertible {
    public var doubleValue: CDouble {
        get {
            return self
        }
    }
}

extension Float : NMBDoubleConvertible {
    public var doubleValue: CDouble {
        get {
            return CDouble(self)
        }
    }
}
#endif

extension NSNumber : NMBDoubleConvertible {
}

private let dateFormatter: NSDateFormatter = {
    let formatter = NSDateFormatter()
    formatter.dateFormat = "yyyy-MM-dd HH:mm:ss.SSSS"
    formatter.locale = NSLocale(localeIdentifier: "en_US_POSIX")

    return formatter
}()

#if _runtime(_ObjC)
extension NSDate: NMBDoubleConvertible {
    public var doubleValue: CDouble {
        get {
            return self.timeIntervalSinceReferenceDate
        }
    }
}
#endif

extension NSDate: TestOutputStringConvertible {
    public var testDescription: String {
        return dateFormatter.stringFromDate(self)
    }
}

/// Protocol for types to support beLessThan(), beLessThanOrEqualTo(),
///  beGreaterThan(), beGreaterThanOrEqualTo(), and equal() matchers.
///
/// Types that conform to Swift's Comparable protocol will work implicitly too
#if _runtime(_ObjC)
@objc public protocol NMBComparable {
    func NMB_compare(otherObject: NMBComparable!) -> NSComparisonResult
}
#else
// This should become obsolete once Corelibs Foundation adds Comparable conformance to NSNumber
public protocol NMBComparable {
    func NMB_compare(otherObject: NMBComparable!) -> NSComparisonResult
}
#endif

extension NSNumber : NMBComparable {
    public func NMB_compare(otherObject: NMBComparable!) -> NSComparisonResult {
        return compare(otherObject as! NSNumber)
    }
}
extension NSString : NMBComparable {
    public func NMB_compare(otherObject: NMBComparable!) -> NSComparisonResult {
        return compare(otherObject as! String)
    }
}
