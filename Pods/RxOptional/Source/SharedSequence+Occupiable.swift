import Foundation
import RxCocoa

public extension SharedSequenceConvertibleType where Element: Occupiable {
    /**
     Filters out empty elements.

     - returns: `Driver` of source `Driver`'s elements, with empty elements filtered out.
     */
    
    func filterEmpty() -> SharedSequence<SharingStrategy, Element> {
        return flatMap { element -> SharedSequence<SharingStrategy, Element> in
            guard element.isNotEmpty else {
                return SharedSequence<SharingStrategy, Element>.empty()
            }
            return SharedSequence<SharingStrategy, Element>.just(element)
        }
    }

    /**
     Replaces empty elements with result returned by `handler`.

     - parameter handler: empty handler function that returns `Driver` of non-empty elements.

     - returns: `Driver` of the source `Driver`'s elements, with empty elements replaced by the handler's returned non-empty elements.
     */
    
    func catchOnEmpty(_ handler: @escaping () -> SharedSequence<SharingStrategy, Element>) -> SharedSequence<SharingStrategy, Element> {
        return flatMap { element -> SharedSequence<SharingStrategy, Element> in
            guard element.isNotEmpty else {
                return handler()
            }
            return SharedSequence<SharingStrategy, Element>.just(element)
        }
    }
}
