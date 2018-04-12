/**
 *  https://github.com/tadija/AEXML
 *  Copyright (c) Marko Tadić 2014-2018
 *  Licensed under the MIT license. See LICENSE file.
 */

import Foundation

/**
    This is base class for holding XML structure.

    You can access its structure by using subscript like this: `element["foo"]["bar"]` which would
    return `<bar></bar>` element from `<element><foo><bar></bar></foo></element>` XML as an `AEXMLElement` object.
*/
open class AEXMLElement {
    
    // MARK: - Properties
    
    /// Every `AEXMLElement` should have its parent element instead of `AEXMLDocument` which parent is `nil`.
    open internal(set) weak var parent: AEXMLElement?
    
    /// Child XML elements.
    open internal(set) var children = [AEXMLElement]()
    
    /// XML Element name.
    open var name: String
    
    /// XML Element value.
    open var value: String?
    
    /// XML Element attributes.
    open var attributes: [String : String]
    
    /// Error value (`nil` if there is no error).
    open var error: AEXMLError?
    
    /// String representation of `value` property (if `value` is `nil` this is empty String).
    open var string: String { return value ?? String() }
    
    /// Boolean representation of `value` property (`nil` if `value` can't be represented as Bool).
    open var bool: Bool? { return Bool(string) }
    
    /// Integer representation of `value` property (`nil` if `value` can't be represented as Integer).
    open var int: Int? { return Int(string) }
    
    /// Double representation of `value` property (`nil` if `value` can't be represented as Double).
    open var double: Double? { return Double(string) }
    
    // MARK: - Lifecycle
    
    /**
        Designated initializer - all parameters are optional.
    
        - parameter name: XML element name.
        - parameter value: XML element value (defaults to `nil`).
        - parameter attributes: XML element attributes (defaults to empty dictionary).
    
        - returns: An initialized `AEXMLElement` object.
    */
    public init(name: String, value: String? = nil, attributes: [String : String] = [String : String]()) {
        self.name = name
        self.value = value
        self.attributes = attributes
    }
    
    // MARK: - XML Read
    
    /// The first element with given name **(Empty element with error if not exists)**.
    open subscript(key: String) -> AEXMLElement {
        guard let
            first = children.first(where: { $0.name == key })
        else {
            let errorElement = AEXMLElement(name: key)
            errorElement.error = AEXMLError.elementNotFound
            return errorElement
        }
        return first
    }
    
    /// Returns all of the elements with equal name as `self` **(nil if not exists)**.
    open var all: [AEXMLElement]? { return parent?.children.filter { $0.name == self.name } }
    
    /// Returns the first element with equal name as `self` **(nil if not exists)**.
    open var first: AEXMLElement? { return all?.first }
    
    /// Returns the last element with equal name as `self` **(nil if not exists)**.
    open var last: AEXMLElement? { return all?.last }
    
    /// Returns number of all elements with equal name as `self`.
    open var count: Int { return all?.count ?? 0 }
    
    /**
        Returns all elements with given value.
        
        - parameter value: XML element value.
        
        - returns: Optional Array of found XML elements.
    */
    open func all(withValue value: String) -> [AEXMLElement]? {
        let found = all?.compactMap {
            $0.value == value ? $0 : nil
        }
        return found
    }
    
    /**
        Returns all elements containing given attributes.

        - parameter attributes: Array of attribute names.

        - returns: Optional Array of found XML elements.
    */
    open func all(containingAttributeKeys keys: [String]) -> [AEXMLElement]? {
        let found = all?.compactMap { element in
            keys.reduce(true) { (result, key) in
                result && Array(element.attributes.keys).contains(key)
            } ? element : nil
        }
        return found
    }
    
    /**
        Returns all elements with given attributes.
    
        - parameter attributes: Dictionary of Keys and Values of attributes.
    
        - returns: Optional Array of found XML elements.
    */
    open func all(withAttributes attributes: [String : String]) -> [AEXMLElement]? {
        let keys = Array(attributes.keys)
        let found = all(containingAttributeKeys: keys)?.compactMap { element in
            attributes.reduce(true) { (result, attribute) in
                result && element.attributes[attribute.key] == attribute.value
            } ? element : nil
        }
        return found
    }
    
    /**
        Returns all descendant elements which satisfy the given predicate.
     
        Searching is done vertically; children are tested before siblings. Elements appear in the list
        in the order in which they are found.
     
        - parameter predicate: Function which returns `true` for a desired element and `false` otherwise.
     
        - returns: Array of found XML elements.
    */
    open func allDescendants(where predicate: (AEXMLElement) -> Bool) -> [AEXMLElement] {
        var result: [AEXMLElement] = []
        
        for child in children {
            if predicate(child) {
                result.append(child)
            }
            result.append(contentsOf: child.allDescendants(where: predicate))
        }
        
        return result
    }
    
    /**
        Returns the first descendant element which satisfies the given predicate, or nil if no such element is found.
     
        Searching is done vertically; children are tested before siblings.
     
        - parameter predicate: Function which returns `true` for the desired element and `false` otherwise.
     
        - returns: Optional AEXMLElement.
    */
    open func firstDescendant(where predicate: (AEXMLElement) -> Bool) -> AEXMLElement? {
        for child in children {
            if predicate(child) {
                return child
            } else if let descendant = child.firstDescendant(where: predicate) {
                return descendant
            }
        }
        return nil
    }
    
    /**
        Indicates whether the element has a descendant satisfying the given predicate.
     
        - parameter predicate: Function which returns `true` for the desired element and `false` otherwise.
     
        - returns: Bool.
    */
    open func hasDescendant(where predicate: (AEXMLElement) -> Bool) -> Bool {
        return firstDescendant(where: predicate) != nil
    }
    
    // MARK: - XML Write
    
    /**
        Adds child XML element to `self`.
    
        - parameter child: Child XML element to add.
    
        - returns: Child XML element with `self` as `parent`.
    */
    @discardableResult open func addChild(_ child: AEXMLElement) -> AEXMLElement {
        child.parent = self
        children.append(child)
        return child
    }
    
    /**
        Adds child XML element to `self`.
        
        - parameter name: Child XML element name.
        - parameter value: Child XML element value (defaults to `nil`).
        - parameter attributes: Child XML element attributes (defaults to empty dictionary).
        
        - returns: Child XML element with `self` as `parent`.
    */
    @discardableResult open func addChild(name: String,
                       value: String? = nil,
                       attributes: [String : String] = [String : String]()) -> AEXMLElement
    {
        let child = AEXMLElement(name: name, value: value, attributes: attributes)
        return addChild(child)
    }
    
    /**
        Adds an array of XML elements to `self`.
    
        - parameter children: Child XML element array to add.
    
        - returns: Child XML elements with `self` as `parent`.
    */
    @discardableResult open func addChildren(_ children: [AEXMLElement]) -> [AEXMLElement] {
        children.forEach{ addChild($0) }
        return children
    }
    
    /// Removes `self` from `parent` XML element.
    open func removeFromParent() {
        parent?.removeChild(self)
    }
    
    fileprivate func removeChild(_ child: AEXMLElement) {
        if let childIndex = children.index(where: { $0 === child }) {
            children.remove(at: childIndex)
        }
    }
    
    fileprivate var parentsCount: Int {
        var count = 0
        var element = self
        
        while let parent = element.parent {
            count += 1
            element = parent
        }
        
        return count
    }
    
    fileprivate func indent(withDepth depth: Int) -> String {
        var count = depth
        var indent = String()
        
        while count > 0 {
            indent += "\t"
            count -= 1
        }
        
        return indent
    }
    
    /// Complete hierarchy of `self` and `children` in **XML** escaped and formatted String
    open var xml: String {
        var xml = String()
        
        // open element
        xml += indent(withDepth: parentsCount - 1)
        xml += "<\(name)"
        
        if attributes.count > 0 {
            // insert attributes
            for (key, value) in attributes {
                xml += " \(key)=\"\(value.xmlEscaped)\""
            }
        }
        
        if value == nil && children.count == 0 {
            // close element
            xml += " />"
        } else {
            if children.count > 0 {
                // add children
                xml += ">\n"
                for child in children {
                    xml += "\(child.xml)\n"
                }
                // add indentation
                xml += indent(withDepth: parentsCount - 1)
                xml += "</\(name)>"
            } else {
                // insert string value and close element
                xml += ">\(string.xmlEscaped)</\(name)>"
            }
        }
        
        return xml
    }
    
    /// Same as `xmlString` but without `\n` and `\t` characters
    open var xmlCompact: String {
        let chars = CharacterSet(charactersIn: "\n\t")
        return xml.components(separatedBy: chars).joined(separator: "")
    }
    
    /// Same as `xmlString` but with 4 spaces instead '\t' characters
    open var xmlSpaces: String {
        let chars = CharacterSet(charactersIn: "\t")
        return xml.components(separatedBy: chars).joined(separator: "    ")
    }
}

public extension String {
    
    /// String representation of self with XML special characters escaped.
    public var xmlEscaped: String {
        // we need to make sure "&" is escaped first. Not doing this may break escaping the other characters
        var escaped = replacingOccurrences(of: "&", with: "&amp;", options: .literal)
        
        // replace the other four special characters
        let escapeChars = ["<" : "&lt;", ">" : "&gt;", "'" : "&apos;", "\"" : "&quot;"]
        for (char, echar) in escapeChars {
            escaped = escaped.replacingOccurrences(of: char, with: echar, options: .literal)
        }
        
        return escaped
    }
    
}
