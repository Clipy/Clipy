import AppKit

@testable import AlternativeMenu

extension AlternativeMenuKeyEquivalent {
    @MainActor
    static func display(for key: String, modifiers: NSEvent.ModifierFlags) -> Display {
        guard !key.isEmpty else {
            return .empty
        }
        let item = NSMenuItem(title: "", action: nil, keyEquivalent: key)
        item.keyEquivalentModifierMask = modifiers
        return display(for: item)
    }
}
