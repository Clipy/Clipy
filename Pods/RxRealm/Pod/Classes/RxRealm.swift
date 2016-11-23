//
//  RxRealm extensions
//
//  Copyright (c) 2016 RxSwiftCommunity. All rights reserved.
//

import Foundation
import RealmSwift
import RxSwift

public enum RxRealmError: Error {
    case objectDeleted
}

//MARK: Realm Collections type extensions

/**
 `NotificationEmitter` is a protocol to allow for Realm's collections to be handled in a generic way.
 
  All collections already include a `addNotificationBlock(_:)` method - making them conform to `NotificationEmitter` just makes it easier to add Rx methods to them.
 
  The methods of essence in this protocol are `asObservable(...)`, which allow for observing for changes on Realm's collections.
*/
public protocol NotificationEmitter {

    associatedtype ElementType

    /**
     Returns a `NotificationToken`, which while retained enables change notifications for the current collection.
     
     - returns: `NotificationToken` - retain this value to keep notifications being emitted for the current collection.
     */
    func addNotificationBlock(_ block: @escaping (RealmCollectionChange<Self>) -> ()) -> NotificationToken

    func toArray() -> [ElementType]
}

extension List: NotificationEmitter {
    public typealias ElementType = Element
    public func toArray() -> [Element] {
        return Array(self)
    }
}

extension AnyRealmCollection: NotificationEmitter {
    public typealias ElementType = Element
    public func toArray() -> [Element] {
        return Array(self)
    }
}

extension Results: NotificationEmitter {
    public typealias ElementType = Element
    public func toArray() -> [Element] {
        return Array(self)
    }
}

extension LinkingObjects: NotificationEmitter {
    public typealias ElementType = Element
    public func toArray() -> [Element] {
        return Array(self)
    }
}

/**
 `RealmChangeset` is a struct that contains the data about a single realm change set. 
 
 It includes the insertions, modifications, and deletions indexes in the data set that the current notification is about.
*/
public struct RealmChangeset {
    /// the indexes in the collection that were deleted
    public let deleted: [Int]
    
    /// the indexes in the collection that were inserted
    public let inserted: [Int]
    
    /// the indexes in the collection that were modified
    public let updated: [Int]
}

public extension ObservableType where E: NotificationEmitter {

    /**
     Returns an `Observable<Self>` that emits each time the collection data changes. The observable emits an initial value upon subscription.

     - returns: `Observable<Self>`, e.g. when called on `Results<Model>` it will return `Observable<Results<Model>>`, on a `List<User>` it will return `Observable<List<User>>`, etc.
     */

    public static func from(_ collection: E, scheduler: ImmediateSchedulerType = CurrentThreadScheduler.instance) -> Observable<E> {
        return Observable.create {observer in
            let token = collection.addNotificationBlock {changeset in

                let value: E

                switch changeset {
                    case .initial(let latestValue):
                        value = latestValue

                    case .update(let latestValue, _, _, _):
                        value = latestValue

                    case .error(let error):
                        observer.onError(error)
                        return
                }

                observer.onNext(value)
            }

            return Disposables.create {
                observer.onCompleted()
                token.stop()
            }
        }
    }

    public static func arrayFrom(_ collection: E, scheduler: ImmediateSchedulerType = CurrentThreadScheduler.instance) -> Observable<Array<E.ElementType>> {
        return Observable.from(collection, scheduler: scheduler)
            .map { $0.toArray() }
    }

    /**
     Returns an `Observable<(Self, RealmChangeset?)>` that emits each time the collection data changes. The observable emits an initial value upon subscription.

     When the observable emits for the first time (if the initial notification is not coalesced with an update) the second tuple value will be `nil`.

     Each following emit will include a `RealmChangeset` with the indexes inserted, deleted or modified.

     - returns: `Observable<(Self, RealmChangeset?)>`
     */
    public static func changesetFrom(_ collection: E, scheduler: ImmediateSchedulerType = CurrentThreadScheduler.instance) -> Observable<(E, RealmChangeset?)> {
        return Observable.create {observer in
            let token = collection.addNotificationBlock {changeset in

                switch changeset {
                    case .initial(let value):
                        observer.onNext((value, nil))
                    case .update(let value, let deletes, let inserts, let updates):
                        observer.onNext((value, RealmChangeset(deleted: deletes, inserted: inserts, updated: updates)))
                    case .error(let error):
                        observer.onError(error)
                        return
                }
            }

            return Disposables.create {
                observer.onCompleted()
                token.stop()
            }
        }
    }

    /**
     Returns an `Observable<(Array<Self.Generator.Element>, RealmChangeset?)>` that emits each time the collection data changes. The observable emits an initial value upon subscription.

     This method emits an `Array` containing all the realm collection objects, this means they all live in the memory. If you're using this method to observe large collections you might hit memory warnings.

     When the observable emits for the first time (if the initial notification is not coalesced with an update) the second tuple value will be `nil`.

     Each following emit will include a `RealmChangeset` with the indexes inserted, deleted or modified.

     - returns: `Observable<(Array<Self.Generator.Element>, RealmChangeset?)>`
     */
    public static func changesetArrayFrom(_ collection: E, scheduler: ImmediateSchedulerType = CurrentThreadScheduler.instance) -> Observable<(Array<E.ElementType>, RealmChangeset?)> {
        return Observable.changesetFrom(collection, scheduler: scheduler)
            .map { ($0.toArray(), $1) }
    }
}

public extension Observable {

    /**
     Returns an `Observable<(Realm, Realm.Notification)>` that emits each time the Realm emits a notification.

     The Observable you will get emits a tuple made out of:

     * the realm that emitted the event
     * the notification type: this can be either `.didChange` which occurs after a refresh or a write transaction ends,
     or `.refreshRequired` which happens when a write transaction occurs from a different thread on the same realm file

     For more information look up: [Realm.Notification](https://realm.io/docs/swift/latest/api/Enums/Notification.html)

     - returns: `Observable<(Realm, Realm.Notification)>`, which you can subscribe to.
     */
    public static func from(_ realm: Realm, scheduler: ImmediateSchedulerType = CurrentThreadScheduler.instance) -> Observable<(Realm, Realm.Notification)> {

        return Observable<(Realm, Realm.Notification)>.create {observer in
            let token = realm.addNotificationBlock {(notification: Realm.Notification, realm: Realm) in
                observer.onNext((realm, notification))
            }

            return Disposables.create {
                observer.onCompleted()
                token.stop()
            }
        }
    }
}

//MARK: Realm type extensions

extension Realm: ReactiveCompatible {}

extension Reactive where Base: Realm {

    /**
     Returns bindable sink wich adds object sequence to the current Realm
     - param: update - if set to `true` it will override existing objects with matching primary key
     - returns: `AnyObserver<O>`, which you can use to subscribe an `Observable` to
     */
    public func add<O: Sequence>(update: Bool = false) -> AnyObserver<O> where O.Iterator.Element: Object {
        return RealmObserver(realm: base) {realm, element in
            try! realm.write {
                realm.add(element, update: update)
            }
        }.asObserver()
    }

    /**
     Returns bindable sink wich adds an object to Realm
     - param: update - if set to `true` it will override existing objects with matching primary key
     - returns: `AnyObserver<O>`, which you can use to subscribe an `Observable` to
     */
    public func add<O: Object>(update: Bool = false) -> AnyObserver<O> {
        return RealmObserver(realm: base) {realm, element in
            try! realm.write {
                realm.add(element, update: update)
            }
        }.asObserver()
    }

    /**
     Returns bindable sink wich deletes objects in sequence from Realm.
     - returns: `AnyObserver<O>`, which you can use to subscribe an `Observable` to
     */
    public func delete<S: Sequence>() -> AnyObserver<S> where S.Iterator.Element: Object {
        return RealmObserver(realm: base, binding: { (realm, elements) in
            try! realm.write {
                realm.delete(elements)
            }
        }).asObserver()
    }

    /**
     Returns bindable sink wich deletes objects in sequence from Realm.
     - returns: `AnyObserver<O>`, which you can use to subscribe an `Observable` to
     */
    public func delete<O: Object>() -> AnyObserver<O> {
        return RealmObserver(realm: base, binding: { (realm, elements) in
            try! realm.write {
                realm.delete(elements)
            }
        }).asObserver()
    }
}

extension Reactive where Base: Realm {
    
    /**
     Returns bindable sink wich adds object sequence to a Realm
     - param: configuration (by default uses `Realm.Configuration.defaultConfiguration`)
     to use to get a Realm for the write operations
     - param: update - if set to `true` it will override existing objects with matching primary key
     - returns: `AnyObserver<O>`, which you can use to subscribe an `Observable` to
     */
    public static func add<O: Sequence>(
        configuration: Realm.Configuration = Realm.Configuration.defaultConfiguration,
        update: Bool = false) -> AnyObserver<O> where O.Iterator.Element: Object {

        return RealmObserver(configuration: configuration) {realm, elements in
            try! realm.write {
                realm.add(elements, update: update)
            }
        }.asObserver()
    }

    /**
     Returns bindable sink wich adds an object to a Realm
     - param: configuration (by default uses `Realm.Configuration.defaultConfiguration`)
     to use to get a Realm for the write operations
     - param: update - if set to `true` it will override existing objects with matching primary key
     - returns: `AnyObserver<O>`, which you can use to subscribe an `Observable` to
     */
    public static func add<O: Object>(
        configuration: Realm.Configuration = Realm.Configuration.defaultConfiguration,
        update: Bool = false) -> AnyObserver<O> {

        return RealmObserver(configuration: configuration) {realm, element in
            try! realm.write {
                realm.add(element, update: update)
            }
        }.asObserver()
    }

    /**
     Returns bindable sink wich deletes objects in sequence from Realm.
     - returns: `AnyObserver<O>`, which you can use to subscribe an `Observable` to
     */
    public static func delete<S: Sequence>() -> AnyObserver<S>  where S.Iterator.Element: Object {
        return AnyObserver {event in

            guard let elements = event.element,
                var generator = elements.makeIterator() as S.Iterator?,
                let first = generator.next(),
                let realm = first.realm else {
                    return
            }

            try! realm.write {
                realm.delete(elements)
            }
        }
    }

    /**
     Returns bindable sink wich deletes object from Realm
     - returns: `AnyObserver<O>`, which you can use to subscribe an `Observable` to
     */
    public static func delete<O: Object>() -> AnyObserver<O> {
        return AnyObserver {event in

            guard let element = event.element,
                let realm = element.realm else {
                    return
            }
            
            try! realm.write {
                realm.delete(element)
            }
        }
    }
}

//MARK: Realm Object type extensions

public extension ObservableType where E: Object {

    public static func from(_ object: E) -> Observable<E> {

        guard let realm = object.realm else {
            return Observable<E>.empty()
        }
        guard let primaryKeyName = type(of: object).primaryKey(),
            let primaryKey = object.value(forKey: primaryKeyName) else {
            fatalError("At present you can't observe objects that don't have primary key.")
        }

        return Observable<E>.create {observer in
            let objectQuery = realm.objects(type(of: object))
                .filter("%K == %@", primaryKeyName, primaryKey)

            let token = objectQuery.addNotificationBlock {changes in
                switch changes {
                case .initial(let results):
                    if let latestObject = results.first {
                        observer.onNext(latestObject)
                    } else {
                        observer.onError(RxRealmError.objectDeleted)
                    }
                case .update(let results, _, _, _):
                    if let latestObject = results.first {
                        observer.onNext(latestObject)
                    } else {
                        observer.onError(RxRealmError.objectDeleted)
                    }
                case .error(let error):
                    observer.onError(error)
                }
            }

            return Disposables.create {
                token.stop()
            }
        }
    }
}
