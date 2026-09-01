//
//  NSMenu+Highlight.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Shunsuke Furubayashi on 2026/06/29.
//
//  Copyright © 2015-2026 Clipy Project.
//

import AppKit

/// Uses a private NSMenu API to highlight the first selectable item by default.
/// ref: https://kazakov.life/2017/05/18/hacking-nsmenu-keyboard-navigation/
extension NSMenu {
    func addSearchField() -> NSSearchField {
        let container = NSView(frame: NSRect(x: 0, y: 0, width: 340, height: 34))
        let searchField = NSSearchField(frame: NSRect(x: 8, y: 4, width: 324, height: 26))
        searchField.autoresizingMask = [.width]
        searchField.placeholderString = String(localized: "Search Clipboard History")
        container.addSubview(searchField)

        let item = NSMenuItem()
        item.view = container
        addItem(item)
        return searchField
    }

    func highlightingFirstItemIfPossible() {
        guard let firstItem = items.first(where: { !$0.isSeparatorItem && !$0.isHidden && $0.isEnabled && $0.view == nil }) else { return }

        let selector = Selector(("highlightItem:"))
        guard responds(to: selector) else { return }

        RunLoop.current.perform(inModes: [.eventTracking]) { [weak self, weak firstItem] in
            self?.perform(selector, with: firstItem)
        }
    }
}
