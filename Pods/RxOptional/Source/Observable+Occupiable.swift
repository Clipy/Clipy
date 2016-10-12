import Foundation
import RxSwift

public extension ObservableType where E: Occupiable {
    /**
     Filter out empty occupibales.

     - returns: Observbale of only non-empty occupiables.
     */
    @warn_unused_result(message="http://git.io/rxs.uo")
    public func filterEmpty() -> Observable<E> {
        return self.flatMap { element -> Observable<E> in
            guard element.isNotEmpty else {
                return Observable<E>.empty()
            }
            return Observable<E>.just(element)
        }
    }

    /**
     When empty uses handler to call another Observbale otherwise passes elemets.

     - parameter handler: Empty handler function, producing another observable.

     - returns: An observable sequence containing the source sequence's elements,
     followed by the elements produced by the handler's resulting observable
     sequence when element was empty.
     */
    @warn_unused_result(message="http://git.io/rxs.uo")
    public func catchOnEmpty(handler: () throws -> Observable<E>) -> Observable<E> {
        return self.flatMap { element -> Observable<E> in
            guard element.isNotEmpty else {
                return try handler()
            }
            return Observable<E>.just(element)
        }
    }

    /**
     Passes value if not empty. When empty throws error.

     - parameter error: Error to throw when empty. Defaults to
     `RxOptionalError.EmptyOccupiable`.

     - returns: Observable containing the source sequence's elements,
     or error if empty.
     */
    @warn_unused_result(message="http://git.io/rxs.uo")
    public func errorOnEmpty(error: ErrorType = RxOptionalError.EmptyOccupiable(E.self)) -> Observable<E> {
        return self.map { element in
            guard element.isNotEmpty else {
                throw error
            }
            return element
        }
    }
}