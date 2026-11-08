//
//  AlternativeMenuKeyEquivalentMatcher.swift
//  AlternativeMenu
//
//  Copyright © 2015-2026 Clipy Project.
//

import AppKit

/// Resolves shortcuts with an undisplayed NSMenu before the session runs an action.
@MainActor
final class AlternativeMenuKeyEquivalentMatcher: NSObject {
    struct Match {
        let handled: Bool
        let item: AlternativeMenuItem?
        let path: [Int]?
    }

    private var menu: NSMenu?
    private var itemsByPath: [[Int]: NSMenuItem] = [:]
    private var matchedItem: (item: AlternativeMenuItem, path: [Int])?

    func updateItems(_ items: [AlternativeMenuItem]) {
        matchedItem = nil
        itemsByPath.removeAll()
        guard !items.isEmpty else {
            menu = nil
            return
        }
        let menu = NSMenu()
        menu.autoenablesItems = false
        append(items, under: [], to: menu)
        self.menu = menu.numberOfItems == 0 ? nil : menu
        // Querying the children primes AppKit's shortcut adjustments on tested systems.
        // AppKit does not guarantee this effect of the accessibility API.
        _ = self.menu?.accessibilityChildren()
        // Native popup tracking retains its prepared shortcuts until it closes.
        // Keep these same items fixed for both display and matching as well.
        for item in menu.items {
            let key = item.keyEquivalent
            let modifiers = item.keyEquivalentModifierMask
            item.allowsAutomaticKeyEquivalentLocalization = false
            item.allowsAutomaticKeyEquivalentMirroring = false
            item.keyEquivalent = key
            item.keyEquivalentModifierMask = modifiers
        }
    }

    func display(at path: [Int]) -> AlternativeMenuKeyEquivalent.Display? {
        guard let item = itemsByPath[path] else { return nil }
        return AlternativeMenuKeyEquivalent.display(for: item)
    }

    func match(_ event: NSEvent) -> Match {
        guard event.type == .keyDown, let menu else { return .init(handled: false, item: nil, path: nil) }
        matchedItem = nil
        defer { matchedItem = nil }
        let handled = menu.performKeyEquivalent(with: event)
        return .init(handled: handled, item: matchedItem?.item, path: matchedItem?.path)
    }

    private func append(_ items: [AlternativeMenuItem], under parentPath: [Int], to menu: NSMenu, ancestorsAreEnabled: Bool = true) {
        for (index, item) in items.enumerated() {
            let isEnabled = ancestorsAreEnabled && item.isEnabled
            if let submenu = item.submenu {
                append(submenu.items, under: parentPath + [index], to: menu, ancestorsAreEnabled: isEnabled)
                continue
            }
            guard !item.keyEquivalent.isEmpty else { continue }
            // The title is unused: this menu is never shown or registered in the menu bar.
            let nativeItem = NSMenuItem(title: "", action: #selector(recordMatch(_:)), keyEquivalent: item.keyEquivalent)
            nativeItem.keyEquivalentModifierMask = item.keyEquivalentModifierMask
            nativeItem.allowsAutomaticKeyEquivalentLocalization = true
            nativeItem.allowsAutomaticKeyEquivalentMirroring = true
            nativeItem.isEnabled = isEnabled
            nativeItem.target = self
            nativeItem.representedObject = item
            menu.addItem(nativeItem)
            itemsByPath[parentPath + [index]] = nativeItem
        }
    }

    @objc private func recordMatch(_ sender: NSMenuItem) {
        guard sender.menu === menu, let item = sender.representedObject as? AlternativeMenuItem,
              let path = itemsByPath.first(where: { $0.value === sender })?.key else { return }
        matchedItem = (item, path)
    }
}
