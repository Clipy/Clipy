import Foundation
import RxCocoa

public extension SharedSequenceConvertibleType where SharingStrategy == DriverSharingStrategy, E: Occupiable {
    /**
     Filters out empty elements.

     - returns: `Driver` of source `Driver`'s elements, with empty elements filtered out.
     */
    
    public func filterEmpty() -> Driver<E> {
        return self.flatMap { element -> Driver<E> in
            guard element.isNotEmpty else {
                return Driver<E>.empty()
            }
            return Driver<E>.just(element)
        }
    }

    /**
     Replaces empty elements with result returned by `handler`.

     - parameter handler: empty handler function that returns `Driver` of non-empty elements.

     - returns: `Driver` of the source `Driver`'s elements, with empty elements replaced by the handler's returned non-empty elements.
     */
    
    public func catchOnEmpty(_ handler: @escaping () -> Driver<E>) -> Driver<E> {
        return self.flatMap { element -> Driver<E> in
            guard element.isNotEmpty else {
                return handler()
            }
            return Driver<E>.just(element)
        }
    }
}
