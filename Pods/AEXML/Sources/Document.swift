//
// Document.swift
//
// Copyright (c) 2014-2016 Marko Tadić <tadija@me.com> http://tadija.net
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

import Foundation

/**
    This class is inherited from `AEXMLElement` and has a few addons to represent **XML Document**.

    XML Parsing is also done with this object.
*/
open class AEXMLDocument: AEXMLElement {
    
    // MARK: - Properties
    
    /// Root (the first child element) element of XML Document **(Empty element with error if not exists)**.
    open var root: AEXMLElement {
        guard let rootElement = children.first else {
            let errorElement = AEXMLElement(name: "Error")
            errorElement.error = AEXMLError.rootElementMissing
            return errorElement
        }
        return rootElement
    }
    
    open let options: AEXMLOptions
    
    // MARK: - Lifecycle
    
    /**
        Designated initializer - Creates and returns new XML Document object.
     
        - parameter root: Root XML element for XML Document (defaults to `nil`).
        - parameter options: Options for XML Document header and parser settings (defaults to `AEXMLOptions()`).
    
        - returns: Initialized XML Document object.
    */
    public init(root: AEXMLElement? = nil, options: AEXMLOptions = AEXMLOptions()) {
        self.options = options
        
        let documentName = String(describing: AEXMLDocument.self)
        super.init(name: documentName)
        
        // document has no parent element
        parent = nil
        
        // add root element to document (if any)
        if let rootElement = root {
            _ = addChild(rootElement)
        }
    }
    
    /**
        Convenience initializer - used for parsing XML data (by calling `loadXMLData:` internally).
     
        - parameter xmlData: XML data to parse.
        - parameter options: Options for XML Document header and parser settings (defaults to `AEXMLOptions()`).
    
        - returns: Initialized XML Document object containing parsed data. Throws error if data could not be parsed.
    */
    public convenience init(xml: Data, options: AEXMLOptions = AEXMLOptions()) throws {
        self.init(options: options)
        try loadXML(xml)
    }
    
    /**
        Convenience initializer - used for parsing XML string (by calling `init(xmlData:options:)` internally).

        - parameter xmlString: XML string to parse.
        - parameter encoding: String encoding for creating `Data` from `xmlString` (defaults to `String.Encoding.utf8`)
        - parameter options: Options for XML Document header and parser settings (defaults to `AEXMLOptions()`).

        - returns: Initialized XML Document object containing parsed data. Throws error if data could not be parsed.
    */
    public convenience init(xml: String,
                            encoding: String.Encoding = String.Encoding.utf8,
                            options: AEXMLOptions = AEXMLOptions()) throws
    {
        guard let data = xml.data(using: encoding) else { throw AEXMLError.parsingFailed }
        try self.init(xml: data, options: options)
    }
    
    // MARK: - Parse XML
    
    /**
        Creates instance of `AEXMLParser` (private class which is simple wrapper around `XMLParser`)
        and starts parsing the given XML data. Throws error if data could not be parsed.
    
        - parameter data: XML which should be parsed.
    */
    open func loadXML(_ data: Data) throws {
        children.removeAll(keepingCapacity: false)
        let xmlParser = AEXMLParser(document: self, data: data)
        try xmlParser.parse()
    }
    
    // MARK: - Override
    
    /// Override of `xml` property of `AEXMLElement` - it just inserts XML Document header at the beginning.
    open override var xml: String {
        var xml =  "\(options.documentHeader.xmlString)\n"
        for child in children {
            xml += child.xml
        }
        return xml
    }
    
}
