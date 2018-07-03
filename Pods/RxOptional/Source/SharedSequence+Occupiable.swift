import Foundation
import RxCocoa

public extension SharedSequenceConvertibleType where E: Occupiable {
    /**
     Filters out empty elements.

     - returns: `Driver` of source `Driver`'s elements, with empty elements filtered out.
     */
    
    public func filterEmpty() -> SharedSequence<SharingStrategy,E> {
        return flatMap { element -> SharedSequence<SharingStrategy,E> in
            guard element.isNotEmpty else {
                return SharedSequence<SharingStrategy,E>.empty()
            }
            return SharedSequence<SharingStrategy,E>.just(element)
        }
    }

    /**
     Replaces empty elements with result returned by `handler`.

     - parameter handler: empty handler function that returns `Driver` of non-empty elements.

     - returns: `Driver` of the source `Driver`'s elements, with empty elements replaced by the handler's returned non-empty elements.
     */
    
    public func catchOnEmpty(_ handler: @escaping () -> SharedSequence<SharingStrategy,E>) -> SharedSequence<SharingStrategy,E> {
        return flatMap { element -> SharedSequence<SharingStrategy,E> in
            guard element.isNotEmpty else {
                return handler()
            }
            return SharedSequence<SharingStrategy,E>.just(element)
        }
    }
}
