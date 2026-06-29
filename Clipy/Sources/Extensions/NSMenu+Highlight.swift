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
    func highlightingfirstItemIfPossible() {
        guard let firstItem = items.first(where: { !$0.isSeparatorItem && !$0.isHidden && $0.isEnabled }) else { return }

        let selector = Selector(("highlightItem:"))
        guard responds(to: selector) else { return }

        RunLoop.current.perform(inModes: [.eventTracking]) { [weak self, weak firstItem] in
            self?.perform(selector, with: firstItem)
        }
    }
}
