import RxCocoa

public extension SharedSequenceConvertibleType where Element: OptionalType {
    /**
     Unwraps and filters out `nil` elements.

     - returns: `Driver` of source `Driver`'s elements, with `nil` elements filtered out.
     */
    
    func filterNil() -> SharedSequence<SharingStrategy, Element.Wrapped> {
        return flatMap { element -> SharedSequence<SharingStrategy, Element.Wrapped> in
            guard let value = element.value else {
                return SharedSequence<SharingStrategy, Element.Wrapped>.empty()
            }
            return SharedSequence<SharingStrategy, Element.Wrapped>.just(value)
        }
    }

    /**
     Unwraps optional elements and replaces `nil` elements with `valueOnNil`.

     - parameter valueOnNil: value to use when `nil` is encountered.

     - returns: `Driver` of the source `Driver`'s unwrapped elements, with `nil` elements replaced by `valueOnNil`.
     */
    
    func replaceNilWith(_ valueOnNil: Element.Wrapped) -> SharedSequence<SharingStrategy, Element.Wrapped> {
        return map { element -> Element.Wrapped in
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
    
    func catchOnNil(_ handler: @escaping () -> SharedSequence<SharingStrategy, Element.Wrapped>) -> SharedSequence<SharingStrategy, Element.Wrapped> {
        return flatMap { element -> SharedSequence<SharingStrategy, Element.Wrapped> in
            guard let value = element.value else {
                return handler()
            }
            return SharedSequence<SharingStrategy, Element.Wrapped>.just(value)
        }
    }
}

#if !swift(>=3.3) || (swift(>=4.0) && !swift(>=4.1))
public extension SharedSequenceConvertibleType where Element: OptionalType, Element.Wrapped: Equatable {
    /**
     Returns an observable sequence that contains only distinct contiguous elements according to equality operator.
     
     - seealso: [distinct operator on reactivex.io](http://reactivex.io/documentation/operators/distinct.html)
     
     - returns: An observable sequence only containing the distinct contiguous elements, based on equality operator, from the source sequence.
     */
    
    func distinctUntilChanged() -> SharedSequence<SharingStrategy, Element> {
        return self.distinctUntilChanged { (lhs, rhs) -> Bool in
            return lhs.value == rhs.value
        }
    }
}
#endif
