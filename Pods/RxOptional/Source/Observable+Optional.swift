import Foundation
import RxSwift

// Some code originally from here: https://github.com/artsy/eidolon/blob/24e36a69bbafb4ef6dbe4d98b575ceb4e1d8345f/Kiosk/Observable%2BOperators.swift#L42-L62
// Credit to Artsy and @ashfurrow

public extension ObservableType where E: OptionalType {
    /**
     Unwrap and filter out nil values.

     - returns: Observbale of only successfully unwrapped values.
     */
    @warn_unused_result(message="http://git.io/rxs.uo")
    public func filterNil() -> Observable<E.Wrapped> {
        return self.flatMap { element -> Observable<E.Wrapped> in
            guard let value = element.value else {
                return Observable<E.Wrapped>.empty()
            }
            return Observable<E.Wrapped>.just(value)
        }
    }

    /**
     Unwraps optional and if finds nil emmits error.

     - parameter error: Error to emit when nil if found. Defaults to
     `RxOptionalError.FoundNilWhileUnwrappingOptional`

     - returns: Observable of unwrapped value or Error.
     */
    @warn_unused_result(message="http://git.io/rxs.uo")
    public func errorOnNil(error: ErrorType = RxOptionalError.FoundNilWhileUnwrappingOptional(E.self)) -> Observable<E.Wrapped> {
        return self.map { element -> E.Wrapped in
            guard let value = element.value else {
                throw error
            }
            return value
        }
    }

    /**
     Unwraps optional and replace nil values with value.

     - parameter valueOnNil: Value to emit when nil is found.

     - returns: Observable of unwrapped value or nilValue.
     */
    @warn_unused_result(message="http://git.io/rxs.uo")
    public func replaceNilWith(valueOnNil: E.Wrapped) -> Observable<E.Wrapped> {
        return self.map { element -> E.Wrapped in
            guard let value = element.value else {
                return valueOnNil
            }
            return value
        }
    }

    /**
     Unwraps element and passes value if not nil. When nil uses handler to call another Observable

     - parameter handler: Nil handler function, producing another Observable.

     - returns: Observable containing the source sequence's elements,
     followed by the elements produced by the handler's resulting observable
     sequence in case an nil was found while unwrapping.
     */
    @warn_unused_result(message="http://git.io/rxs.uo")
    public func catchOnNil(handler: () throws -> Observable<E.Wrapped>) -> Observable<E.Wrapped> {
        return self.flatMap { element -> Observable<E.Wrapped> in
            guard let value = element.value else {
                return try handler()
            }
            return Observable<E.Wrapped>.just(value)
        }
    }
}
