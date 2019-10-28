import Foundation

// Originally from here: https://github.com/artsy/eidolon/blob/f95c0a5bf1e90358320529529d6bf431ada04c3f/Kiosk/App/SwiftExtensions.swift#L23-L40
// Credit to Artsy and @ashfurrow

// Anything that can hold a value (strings, arrays, etc.)
public protocol Occupiable {
    var isEmpty: Bool { get }
    var isNotEmpty: Bool { get }
}

public extension Occupiable {
    var isNotEmpty: Bool {
        return !isEmpty
    }
}

extension String: Occupiable { }
// I can't think of a way to combine these collection types. Suggestions welcomed!
extension Array: Occupiable { }
extension Dictionary: Occupiable { }
extension Set: Occupiable { }
