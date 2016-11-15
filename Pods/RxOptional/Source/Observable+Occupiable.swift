import Foundation
import RxSwift

public extension ObservableType where E: Occupiable {
    /**
     Filter out empty occupiable elements.

     - returns: `Observable` of source `Observable`'s occupiable elements, with empty occupiable elements filtered out.
     */
    
    public func filterEmpty() -> Observable<E> {
        return self.flatMap { element -> Observable<E> in
            guard element.isNotEmpty else {
                return Observable<E>.empty()
            }
            return Observable<E>.just(element)
        }
    }

    /**
     Replaces empty occupiable elements with result returned by `handler`.

     - parameter handler: empty handler throwing function that returns `Observable` of non-empty occupiable elements.

     - returns: `Observable` of the source `Observable`'s occupiable elements, with empty occupiable elements replaced by the handler's returned non-empty occupiable elements.
     */
    
    public func catchOnEmpty(_ handler: @escaping () throws -> Observable<E>) -> Observable<E> {
        return self.flatMap { element -> Observable<E> in
            guard element.isNotEmpty else {
                return try handler()
            }
            return Observable<E>.just(element)
        }
    }

    /**
     Throws an error if the source `Observable` contains an empty occupiable element; otherwise returns original source `Observable` of non-empty occupiable elements.

     - parameter error: error to throw when an empty occupiable element is encountered. Defaults to `RxOptionalError.EmptyOccupiable`.

     - throws: `error` if an empty occupiable element is encountered.

     - returns: original source `Observable` of non-empty occupiable elements if it contains no empty occupiable elements.
     */
    
    public func errorOnEmpty(_ error: Error = RxOptionalError.emptyOccupiable(E.self)) -> Observable<E> {
        return self.map { element in
            guard element.isNotEmpty else {
                throw error
            }
            return element
        }
    }
}
