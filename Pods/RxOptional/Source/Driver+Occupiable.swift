import Foundation
import RxCocoa

public extension Driver where Element: Occupiable {
    /**
     Filter out empty occupibales.

     - returns: Driver of only non-empty occupiables.
     */
    @warn_unused_result(message="http://git.io/rxs.uo")
    public func filterEmpty() -> Driver<Element> {
        return self.flatMap { element -> Driver<Element> in
            guard element.isNotEmpty else {
                return Driver<Element>.empty()
            }
            return Driver<Element>.just(element)
        }
    }

    /**
     When empty uses handler to call another Driver otherwise passes elemets.

     - parameter handler: Empty handler function, producing another Driver.

     - returns: Driver containing the source sequence's elements,
     followed by the elements produced by the handler's resulting observable
     sequence when element was empty.
     */
    @warn_unused_result(message="http://git.io/rxs.uo")
    public func catchOnEmpty(handler: () -> Driver<Element>) -> Driver<Element> {
        return self.flatMap { element -> Driver<Element> in
            guard element.isNotEmpty else {
                return handler()
            }
            return Driver<Element>.just(element)
        }
    }
}
