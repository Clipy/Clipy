/**
 *  https://github.com/tadija/AEXML
 *  Copyright © Marko Tadić 2014-2020
 *  Licensed under the MIT license
 */

import Foundation
#if canImport(FoundationXML)
import FoundationXML
#endif

/// A type representing error value that can be thrown or inside `error` property of `AEXMLElement`.
public enum AEXMLError: Error {
    /// This will be inside `error` property of `AEXMLElement` when subscript is used for not-existing element.
    case elementNotFound
    
    /// This will be inside `error` property of `AEXMLDocument` when there is no root element.
    case rootElementMissing
    
    /// `AEXMLDocument` can throw this error on `init` or `loadXMLData` if parsing with `XMLParser` was not successful.
    case parsingFailed
}
