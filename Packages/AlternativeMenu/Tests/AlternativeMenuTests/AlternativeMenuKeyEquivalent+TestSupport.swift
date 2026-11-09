import AppKit

@testable import AlternativeMenu

extension AlternativeMenuKeyEquivalent {
    @MainActor
    static func display(for key: String, modifiers: NSEvent.ModifierFlags) -> Display {
        let item = NSMenuItem(title: "", action: nil, keyEquivalent: key)
        item.keyEquivalentModifierMask = modifiers
        return display(for: item)
    }
}
