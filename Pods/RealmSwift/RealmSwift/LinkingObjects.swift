////////////////////////////////////////////////////////////////////////////
//
// Copyright 2016 Realm Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
////////////////////////////////////////////////////////////////////////////

import Foundation
import Realm

/// :nodoc:
/// Internal class. Do not use directly. Used for reflection and initialization
public class LinkingObjectsBase: NSObject, NSFastEnumeration {
    internal let objectClassName: String
    internal let propertyName: String

    fileprivate var cachedRLMResults: RLMResults<RLMObject>?
    @objc fileprivate var object: RLMWeakObjectHandle?
    @objc fileprivate var property: RLMProperty?

    internal var rlmResults: RLMResults<RLMObject> {
        if cachedRLMResults == nil {
            if let object = self.object, let property = self.property {
                cachedRLMResults = RLMDynamicGet(object.object, property)! as? RLMResults
                self.object = nil
                self.property = nil
            } else {
                cachedRLMResults = RLMResults.emptyDetached()
            }
        }
        return cachedRLMResults!
    }

    init(fromClassName objectClassName: String, property propertyName: String) {
        self.objectClassName = objectClassName
        self.propertyName = propertyName
    }

    // MARK: Fast Enumeration
    public func countByEnumerating(with state: UnsafeMutablePointer<NSFastEnumerationState>,
                                   objects buffer: AutoreleasingUnsafeMutablePointer<AnyObject?>,
                                   count len: Int) -> Int {
        return Int(rlmResults.countByEnumerating(with: state,
                                                 objects: buffer,
                                                 count: UInt(len)))
    }
}

/**
 `LinkingObjects` is an auto-updating container type. It represents zero or more objects that are linked to its owning
 model object through a property relationship.

 `LinkingObjects` can be queried with the same predicates as `List<T>` and `Results<T>`.

 `LinkingObjects` always reflects the current state of the Realm on the current thread, including during write
 transactions on the current thread. The one exception to this is when using `for...in` enumeration, which will always
 enumerate over the linking objects that were present when the enumeration is begun, even if some of them are deleted or
 modified to no longer link to the target object during the enumeration.

 `LinkingObjects` can only be used as a property on `Object` models. Properties of this type must be declared as `let`
 and cannot be `dynamic`.
 */
public final class LinkingObjects<T: Object>: LinkingObjectsBase {
    /// The type of the objects represented by the linking objects.
    public typealias Element = T

    // MARK: Properties

    /// The Realm which manages the linking objects, or `nil` if the linking objects are unmanaged.
    public var realm: Realm? { return rlmResults.isAttached ? Realm(rlmResults.realm) : nil }

    /// Indicates if the linking objects are no longer valid.
    ///
    /// The linking objects become invalid if `invalidate()` is called on the containing `realm` instance.
    ///
    /// An invalidated linking objects can be accessed, but will always be empty.
    public var isInvalidated: Bool { return rlmResults.isInvalidated }

    /// The number of linking objects.
    public var count: Int { return Int(rlmResults.count) }

    // MARK: Initializers

    /**
     Creates an instance of a `LinkingObjects`. This initializer should only be called when declaring a property on a
     Realm model.

     - parameter type:         The type of the object owning the property the linking objects should refer to.
     - parameter propertyName: The property name of the property the linking objects should refer to.
     */
    public init(fromType type: T.Type, property propertyName: String) {
        let className = (T.self as Object.Type).className()
        super.init(fromClassName: className, property: propertyName)
    }

    /// A human-readable description of the objects represented by the linking objects.
    public override var description: String {
        let type = "LinkingObjects<\(rlmResults.objectClassName)>"
        return gsub(pattern: "RLMResults <0x[a-z0-9]+>", template: type, string: rlmResults.description) ?? type
    }

    // MARK: Index Retrieval

    /**
     Returns the index of an object in the linking objects, or `nil` if the object is not present.

     - parameter object: The object whose index is being queried.
     */
    public func index(of object: T) -> Int? {
        return notFoundToNil(index: rlmResults.index(of: object.unsafeCastToRLMObject()))
    }

    /**
     Returns the index of the first object matching the given predicate, or `nil` if no objects match.

     - parameter predicate: The predicate with which to filter the objects.
     */
    public func index(matching predicate: NSPredicate) -> Int? {
        return notFoundToNil(index: rlmResults.indexOfObject(with: predicate))
    }

    /**
     Returns the index of the first object matching the given predicate, or `nil` if no objects match.

     - parameter predicateFormat: A predicate format string, optionally followed by a variable number of arguments.
     */
    public func index(matching predicateFormat: String, _ args: Any...) -> Int? {
        return notFoundToNil(index: rlmResults.indexOfObject(with: NSPredicate(format: predicateFormat,
                                                                               argumentArray: args)))
    }

    // MARK: Object Retrieval

    /**
     Returns the object at the given `index`.

     - parameter index: The index.
     */
    public subscript(index: Int) -> T {
        get {
            throwForNegativeIndex(index)
            return unsafeBitCast(rlmResults[UInt(index)], to: T.self)
        }
    }

    /// Returns the first object in the linking objects, or `nil` if the linking objects are empty.
    public var first: T? { return unsafeBitCast(rlmResults.firstObject(), to: Optional<T>.self) }

    /// Returns the last object in the linking objects, or `nil` if the linking objects are empty.
    public var last: T? { return unsafeBitCast(rlmResults.lastObject(), to: Optional<T>.self) }

    // MARK: KVC

    /**
     Returns an `Array` containing the results of invoking `valueForKey(_:)` with `key` on each of the linking objects.

     - parameter key: The name of the property whose values are desired.
     */
    public override func value(forKey key: String) -> Any? {
        return value(forKeyPath: key)
    }

    /**
     Returns an `Array` containing the results of invoking `valueForKeyPath(_:)` with `keyPath` on each of the linking
     objects.

     - parameter keyPath: The key path to the property whose values are desired.
     */
    public override func value(forKeyPath keyPath: String) -> Any? {
        return rlmResults.value(forKeyPath: keyPath)
    }

    /**
     Invokes `setValue(_:forKey:)` on each of the linking objects using the specified `value` and `key`.

     - warning: This method may only be called during a write transaction.

     - parameter value: The value to set the property to.
     - parameter key:   The name of the property whose value should be set on each object.
     */
    public override func setValue(_ value: Any?, forKey key: String) {
        return rlmResults.setValue(value, forKeyPath: key)
    }

    // MARK: Filtering

    /**
     Returns a `Results` containing all objects matching the given predicate in the linking objects.

     - parameter predicateFormat: A predicate format string, optionally followed by a variable number of arguments.
     */
    public func filter(_ predicateFormat: String, _ args: Any...) -> Results<T> {
        return Results<T>(rlmResults.objects(with: NSPredicate(format: predicateFormat, argumentArray: args)))
    }

    /**
     Returns a `Results` containing all objects matching the given predicate in the linking objects.

     - parameter predicate: The predicate with which to filter the objects.
     */
    public func filter(_ predicate: NSPredicate) -> Results<T> {
        return Results<T>(rlmResults.objects(with: predicate))
    }

    // MARK: Sorting

    /**
     Returns a `Results` containing all the linking objects, but sorted.

     Objects are sorted based on the values of the given key path. For example, to sort a collection of `Student`s from
     youngest to oldest based on their `age` property, you might call
     `students.sorted(byKeyPath: "age", ascending: true)`.

     - warning: Collections may only be sorted by properties of boolean, `Date`, `NSDate`, single and double-precision
                floating point, integer, and string types.

     - parameter keyPath:  The key path to sort by.
     - parameter ascending: The direction to sort in.
     */
    public func sorted(byKeyPath keyPath: String, ascending: Bool = true) -> Results<T> {
        return sorted(by: [SortDescriptor(keyPath: keyPath, ascending: ascending)])
    }

    /**
     Returns a `Results` containing all the linking objects, but sorted.

     Objects are sorted based on the values of the given property. For example, to sort a collection of `Student`s from
     youngest to oldest based on their `age` property, you might call
     `students.sorted(byProperty: "age", ascending: true)`.

     - warning: Collections may only be sorted by properties of boolean, `Date`, `NSDate`, single and double-precision
                floating point, integer, and string types.

     - parameter property:  The name of the property to sort by.
     - parameter ascending: The direction to sort in.
     */
    @available(*, deprecated, renamed: "sorted(byKeyPath:ascending:)")
    public func sorted(byProperty property: String, ascending: Bool = true) -> Results<T> {
        return sorted(byKeyPath: property, ascending: ascending)
    }

    /**
     Returns a `Results` containing all the linking objects, but sorted.

     - warning: Collections may only be sorted by properties of boolean, `Date`, `NSDate`, single and double-precision
                floating point, integer, and string types.

     - see: `sorted(byKeyPath:ascending:)`

     - parameter sortDescriptors: A sequence of `SortDescriptor`s to sort by.
     */
    public func sorted<S: Sequence>(by sortDescriptors: S) -> Results<T> where S.Iterator.Element == SortDescriptor {
        return Results<T>(rlmResults.sortedResults(using: sortDescriptors.map { $0.rlmSortDescriptorValue }))
    }

    // MARK: Aggregate Operations

    /**
     Returns the minimum (lowest) value of the given property among all the linking objects, or `nil` if the linking
     objects are empty.

     - warning: Only a property whose type conforms to the `MinMaxType` protocol can be specified.

     - parameter property: The name of a property whose minimum value is desired.
     */
    public func min<U: MinMaxType>(ofProperty property: String) -> U? {
        return rlmResults.min(ofProperty: property).map(dynamicBridgeCast)
    }

    /**
     Returns the maximum (highest) value of the given property among all the linking objects, or `nil` if the linking
     objects are empty.

     - warning: Only a property whose type conforms to the `MinMaxType` protocol can be specified.

     - parameter property: The name of a property whose minimum value is desired.
     */
    public func max<U: MinMaxType>(ofProperty property: String) -> U? {
        return rlmResults.max(ofProperty: property).map(dynamicBridgeCast)
    }

    /**
     Returns the sum of the values of a given property over all the linking objects.

     - warning: Only a property whose type conforms to the `AddableType` protocol can be specified.

     - parameter property: The name of a property whose values should be summed.
     */
    public func sum<U: AddableType>(ofProperty property: String) -> U {
        return dynamicBridgeCast(fromObjectiveC: rlmResults.sum(ofProperty: property))
    }

    /**
     Returns the average value of a given property over all the linking objects, or `nil` if the linking objects are
     empty.

     - warning: Only the name of a property whose type conforms to the `AddableType` protocol can be specified.

     - parameter property: The name of a property whose average value should be calculated.
     */
    public func average<U: AddableType>(ofProperty property: String) -> U? {
        return rlmResults.average(ofProperty: property).map(dynamicBridgeCast)
    }

    // MARK: Notifications

    /**
     Registers a block to be called each time the collection changes.

     The block will be asynchronously called with the initial results, and then called again after each write
     transaction which changes either any of the objects in the collection, or which objects are in the collection.

     The `change` parameter that is passed to the block reports, in the form of indices within the collection, which of
     the objects were added, removed, or modified during each write transaction. See the `RealmCollectionChange`
     documentation for more information on the change information supplied and an example of how to use it to update a
     `UITableView`.

     At the time when the block is called, the collection will be fully evaluated and up-to-date, and as long as you do
     not perform a write transaction on the same thread or explicitly call `realm.refresh()`, accessing it will never
     perform blocking work.

     Notifications are delivered via the standard run loop, and so can't be delivered while the run loop is blocked by
     other activity. When notifications can't be delivered instantly, multiple notifications may be coalesced into a
     single notification. This can include the notification with the initial collection.

     For example, the following code performs a write transaction immediately after adding the notification block, so
     there is no opportunity for the initial notification to be delivered first. As a result, the initial notification
     will reflect the state of the Realm after the write transaction.

     ```swift
     let results = realm.objects(Dog.self)
     print("dogs.count: \(dogs?.count)") // => 0
     let token = dogs.addNotificationBlock { changes in
         switch changes {
         case .initial(let dogs):
             // Will print "dogs.count: 1"
             print("dogs.count: \(dogs.count)")
             break
         case .update:
             // Will not be hit in this example
             break
         case .error:
             break
         }
     }
     try! realm.write {
         let dog = Dog()
         dog.name = "Rex"
         person.dogs.append(dog)
     }
     // end of run loop execution context
     ```

     You must retain the returned token for as long as you want updates to be sent to the block. To stop receiving
     updates, call `stop()` on the token.

     - warning: This method cannot be called during a write transaction, or when the containing Realm is read-only.

     - parameter block: The block to be called whenever a change occurs.
     - returns: A token which must be held for as long as you want updates to be delivered.
     */
    public func addNotificationBlock(_ block: @escaping (RealmCollectionChange<LinkingObjects>) -> Void) -> NotificationToken {
        return rlmResults.addNotificationBlock { _, change, error in
            block(RealmCollectionChange.fromObjc(value: self, change: change, error: error))
        }
    }
}

extension LinkingObjects : RealmCollection {
    // MARK: Sequence Support

    /// Returns an iterator that yields successive elements in the linking objects.
    public func makeIterator() -> RLMIterator<T> {
        return RLMIterator(collection: rlmResults)
    }

    // MARK: Collection Support

    /// The position of the first element in a non-empty collection.
    /// Identical to endIndex in an empty collection.
    public var startIndex: Int { return 0 }

    /// The collection's "past the end" position.
    /// endIndex is not a valid argument to subscript, and is always reachable from startIndex by
    /// zero or more applications of successor().
    public var endIndex: Int { return count }

    public func index(after: Int) -> Int {
      return after + 1
    }

    public func index(before: Int) -> Int {
      return before - 1
    }

    /// :nodoc:
    public func _addNotificationBlock(_ block: @escaping (RealmCollectionChange<AnyRealmCollection<T>>) -> Void) ->
        NotificationToken {
            let anyCollection = AnyRealmCollection(self)
            return rlmResults.addNotificationBlock { _, change, error in
                block(RealmCollectionChange.fromObjc(value: anyCollection, change: change, error: error))
            }
    }
}

// MARK: AssistedObjectiveCBridgeable

extension LinkingObjects: AssistedObjectiveCBridgeable {
    internal static func bridging(from objectiveCValue: Any, with metadata: Any?) -> LinkingObjects {
        guard let metadata = metadata as? LinkingObjectsBridgingMetadata else { preconditionFailure() }

        let swiftValue = LinkingObjects(fromType: T.self, property: metadata.propertyName)
        switch (objectiveCValue, metadata) {
        case (let object as RLMObjectBase, .uncached(let property)):
            swiftValue.object = RLMWeakObjectHandle(object: object)
            swiftValue.property = property
        case (let results as RLMResults<RLMObject>, .cached):
            swiftValue.cachedRLMResults = results
        default:
            preconditionFailure()
        }
        return swiftValue
    }

    internal var bridged: (objectiveCValue: Any, metadata: Any?) {
        if let results = cachedRLMResults {
            return (objectiveCValue: results,
                    metadata: LinkingObjectsBridgingMetadata.cached(propertyName: propertyName))
        } else {
            return (objectiveCValue: (object!.copy() as! RLMWeakObjectHandle).object,
                    metadata: LinkingObjectsBridgingMetadata.uncached(property: property!))
        }
    }
}

internal enum LinkingObjectsBridgingMetadata {
    case uncached(property: RLMProperty)
    case cached(propertyName: String)

    fileprivate var propertyName: String {
        switch self {
        case .uncached(let property):   return property.name
        case .cached(let propertyName): return propertyName
        }
    }
}

// MARK: Unavailable

extension LinkingObjects {
    @available(*, unavailable, renamed: "isInvalidated")
    public var invalidated: Bool { fatalError() }

    @available(*, unavailable, renamed: "index(matching:)")
    public func index(of predicate: NSPredicate) -> Int? { fatalError() }

    @available(*, unavailable, renamed: "index(matching:_:)")
    public func index(of predicateFormat: String, _ args: Any...) -> Int? { fatalError() }

    @available(*, unavailable, renamed: "sorted(byKeyPath:ascending:)")
    public func sorted(_ property: String, ascending: Bool = true) -> Results<T> { fatalError() }

    @available(*, unavailable, renamed: "sorted(by:)")
    public func sorted<S: Sequence>(_ sortDescriptors: S) -> Results<T> where S.Iterator.Element == SortDescriptor {
        fatalError()
    }

    @available(*, unavailable, renamed: "min(ofProperty:)")
    public func min<U: MinMaxType>(_ property: String) -> U? { fatalError() }

    @available(*, unavailable, renamed: "max(ofProperty:)")
    public func max<U: MinMaxType>(_ property: String) -> U? { fatalError() }

    @available(*, unavailable, renamed: "sum(ofProperty:)")
    public func sum<U: AddableType>(_ property: String) -> U { fatalError() }

    @available(*, unavailable, renamed: "average(ofProperty:)")
    public func average<U: AddableType>(_ property: String) -> U? { fatalError() }
}
