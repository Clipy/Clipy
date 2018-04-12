import Foundation
import RxSwift

// Some code originally from here: https://github.com/artsy/eidolon/blob/24e36a69bbafb4ef6dbe4d98b575ceb4e1d8345f/Kiosk/Observable%2BOperators.swift#L42-L62
// Credit to Artsy and @ashfurrow

public extension ObservableType where E: OptionalType {
    /**
     Unwraps and filters out `nil` elements.

     - returns: `Observable` of source `Observable`'s elements, with `nil` elements filtered out.
     */
    
    public func filterNil() -> Observable<E.Wrapped> {
        return self.flatMap { element -> Observable<E.Wrapped> in
            guard let value = element.value else {
                return Observable<E.Wrapped>.empty()
            }
            return Observable<E.Wrapped>.just(value)
        }
    }

    /** 
    
    Filters out `nil` elements. Similar to `filterNil`, but keeps the elements of the observable
    wrapped in Optionals. This is often useful for binding to a UIBindingObserver with an optional type.

    - returns: `Observable` of source `Observable`'s elements, with `nil` elements filtered out.
    */

    public func filterNilKeepOptional() -> Observable<E> {
        return self.filter { element -> Bool in
            return element.value != nil
        }
    }

    /**
     Throws an error if the source `Observable` contains an empty element; otherwise returns original source `Observable` of non-empty elements.

     - parameter error: error to throw when an empty element is encountered. Defaults to `RxOptionalError.FoundNilWhileUnwrappingOptional`.

     - throws: `error` if an empty element is encountered.

     - returns: original source `Observable` of non-empty elements if it contains no empty elements.
     */
    
    public func errorOnNil(_ error: Error = RxOptionalError.foundNilWhileUnwrappingOptional(E.self)) -> Observable<E.Wrapped> {
        return self.map { element -> E.Wrapped in
            guard let value = element.value else {
                throw error
            }
            return value
        }
    }

    /**
     Unwraps optional elements and replaces `nil` elements with `valueOnNil`.

     - parameter valueOnNil: value to use when `nil` is encountered.

     - returns: `Observable` of the source `Observable`'s unwrapped elements, with `nil` elements replaced by `valueOnNil`.
     */
    
    public func replaceNilWith(_ valueOnNil: E.Wrapped) -> Observable<E.Wrapped> {
        return self.map { element -> E.Wrapped in
            guard let value = element.value else {
                return valueOnNil
            }
            return value
        }
    }

    /**
     Unwraps optional elements and replaces `nil` elements with result returned by `handler`.

     - parameter handler: `nil` handler throwing function that returns `Observable` of non-`nil` elements.

     - returns: `Observable` of the source `Observable`'s unwrapped elements, with `nil` elements replaced by the handler's returned non-`nil` elements.
     */
    
    public func catchOnNil(_ handler: @escaping () throws -> Observable<E.Wrapped>) -> Observable<E.Wrapped> {
        return self.flatMap { element -> Observable<E.Wrapped> in
            guard let value = element.value else {
                return try handler()
            }
            return Observable<E.Wrapped>.just(value)
        }
    }
}

#if !swift(>=3.3) || (swift(>=4.0) && !swift(>=4.1))
public extension ObservableType where E: OptionalType, E.Wrapped: Equatable {
    /**
     Returns an observable sequence that contains only distinct contiguous elements according to equality operator.
     
     - seealso: [distinct operator on reactivex.io](http://reactivex.io/documentation/operators/distinct.html)
     
     - returns: An observable sequence only containing the distinct contiguous elements, based on equality operator, from the source sequence.
     */
    
    public func distinctUntilChanged() -> Observable<Self.E> {
        return self.distinctUntilChanged { (lhs, rhs) -> Bool in
            return lhs.value == rhs.value
        }
    }
}
#endif
