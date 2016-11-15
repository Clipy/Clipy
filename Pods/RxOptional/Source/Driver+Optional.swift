import RxCocoa

public extension SharedSequenceConvertibleType where SharingStrategy == DriverSharingStrategy, E: OptionalType {
    /**
     Unwraps and filters out `nil` elements.

     - returns: `Driver` of source `Driver`'s elements, with `nil` elements filtered out.
     */
    
    public func filterNil() -> Driver<E.Wrapped> {
        return self.flatMap { element -> Driver<E.Wrapped> in
            guard let value = element.value else {
                return Driver<E.Wrapped>.empty()
            }
            return Driver<E.Wrapped>.just(value)
        }
    }

    /**
     Unwraps optional elements and replaces `nil` elements with `valueOnNil`.

     - parameter valueOnNil: value to use when `nil` is encountered.

     - returns: `Driver` of the source `Driver`'s unwrapped elements, with `nil` elements replaced by `valueOnNil`.
     */
    
    public func replaceNilWith(_ valueOnNil: E.Wrapped) -> Driver<E.Wrapped> {
        return self.map { element -> E.Wrapped in
            guard let value = element.value else {
                return valueOnNil
            }
            return value
        }
    }

    /**
     Unwraps optional elements and replaces `nil` elements with result returned by `handler`.

     - parameter handler: `nil` handler function that returns `Driver` of non-`nil` elements.

     - returns: `Driver` of the source `Driver`'s unwrapped elements, with `nil` elements replaced by the handler's returned non-`nil` elements.
     */
    
    public func catchOnNil(_ handler: @escaping () -> Driver<E.Wrapped>) -> Driver<E.Wrapped> {
        return self.flatMap { element -> Driver<E.Wrapped> in
            guard let value = element.value else {
                return handler()
            }
            return Driver<E.Wrapped>.just(value)
        }
    }
}

public extension SharedSequenceConvertibleType where SharingStrategy == DriverSharingStrategy, E: OptionalType, E.Wrapped: Equatable {
    /**
     Returns an observable sequence that contains only distinct contiguous elements according to equality operator.
     
     - seealso: [distinct operator on reactivex.io](http://reactivex.io/documentation/operators/distinct.html)
     
     - returns: An observable sequence only containing the distinct contiguous elements, based on equality operator, from the source sequence.
     */
    
    public func distinctUntilChanged() -> Driver<E> {
        return self.distinctUntilChanged { (lhs, rhs) -> Bool in
            return lhs.value == rhs.value
        }
    }
}
