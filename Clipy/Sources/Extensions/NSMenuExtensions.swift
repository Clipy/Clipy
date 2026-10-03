//
//  NSMenuExtensions.swift
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

extension NSMenu {
    @discardableResult
    func popUpHighlightingFirstItem(positioning item: NSMenuItem?, at location: NSPoint, in view: NSView?) -> Bool {
        let selector = #selector(highlightingFirstItemIfPossible(_:))
        let notificationCenter = NotificationCenter.default
        let observer = notificationCenter.addObserver(forName: NSMenu.didBeginTrackingNotification, object: self, queue: nil) { [weak self] _ in
            // Delay highlighting to support scrollable menus.
            self?.perform(selector, with: nil, afterDelay: 0.01, inModes: [.eventTracking])
        }

        defer {
            notificationCenter.removeObserver(observer)
            NSObject.cancelPreviousPerformRequests(withTarget: self, selector: selector, object: nil)
        }

        return popUp(positioning: item, at: location, in: view)
    }

    /// Uses a private NSMenu API to highlight the first selectable item.
    /// ref: https://kazakov.life/2017/05/18/hacking-nsmenu-keyboard-navigation/
    @objc
    private func highlightingFirstItemIfPossible(_: Any?) {
        guard let firstItem = items.first(where: { !$0.isSeparatorItem && !$0.isHidden && $0.isEnabled }) else { return }
        guard highlightedItem == nil else { return }

        let selector = NSSelectorFromString("highlightItem:")
        guard responds(to: selector) else { return }

        perform(selector, with: firstItem)
    }
}
