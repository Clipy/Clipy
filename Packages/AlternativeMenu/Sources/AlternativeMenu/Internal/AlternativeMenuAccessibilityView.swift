//
//  AlternativeMenuAccessibilityView.swift
//  AlternativeMenu
//
//  Copyright © 2015-2026 Clipy Project.
//

import AppKit
import SwiftUI

/// Exposes the SwiftUI results as a menu without participating in pointer input.
final class AlternativeMenuAccessibilityView: NSView {
    weak var menuView: AlternativeMenuView?
    private var elements: [Row?] = []

    init() {
        super.init(frame: .zero)
    }

    @available(*, unavailable)
    required init?(coder: NSCoder) { fatalError("init(coder:) is unavailable") }

    func setRows(_ rows: [AlternativeMenuRowData]) {
        elements = rows.enumerated().map { index, row in
            row.kind == .separator ? nil : Row(row, index: index, menu: self)
        }
    }

    override func hitTest(_ point: NSPoint) -> NSView? { nil }
    override func isAccessibilityElement() -> Bool { true }
    override func accessibilityRole() -> NSAccessibility.Role? { .menu }
    override func accessibilityTitle() -> String? { NSAccessibility.Role.menu.description(with: nil) }
    override func accessibilityChildren() -> [Any]? { elements.compactMap { $0 } }

    override func accessibilityVisibleChildren() -> [Any]? {
        elements.compactMap { $0 }.filter { !$0.accessibilityFrame().isEmpty }
    }

    override func accessibilitySelectedChildren() -> [Any]? {
        guard let index = menuView?.selection, elements.indices.contains(index), let element = elements[index] else { return [] }
        return [element]
    }

    nonisolated override func accessibilityHitTest(_ point: NSPoint) -> Any? {
        guard Thread.isMainThread else { return self }
        let row = MainActor.assumeIsolated { () -> Row? in
            guard let menuView, let window = menuView.window,
                  let index = menuView.rowIndex(at: window.convertPoint(fromScreen: point)),
                  elements.indices.contains(index) else { return nil }
            return elements[index]
        }
        return row ?? self as Any
    }

    override func accessibilityPerformCancel() -> Bool {
        guard let menuView, menuView.window?.isVisible == true, let cancel = menuView.onCancelMenu else { return false }
        cancel()
        return true
    }

    private func containsRow(at index: Int, identity: ObjectIdentifier) -> Bool {
        guard elements.indices.contains(index), let row = elements[index] else { return false }
        return ObjectIdentifier(row) == identity
    }

    private final class Row: NSAccessibilityElement, @unchecked Sendable {
        private let index: Int
        private let owner: @MainActor @Sendable () -> AlternativeMenuAccessibilityView?
        private let hasSubmenu: Bool

        @MainActor
        init(_ row: AlternativeMenuRowData, index: Int, menu: AlternativeMenuAccessibilityView) {
            self.index = index
            owner = { [weak menu] in menu }
            hasSubmenu = row.hasSubmenu
            super.init()
            setAccessibilityElement(true)
            setAccessibilityRole(row.kind == .header ? .staticText : .menuItem)
            let title = row.title.string
            setAccessibilityTitle(title)
            setAccessibilityEnabled(row.kind == .item)
            setAccessibilityParent(menu)
            setAccessibilityHelp(row.toolTip ?? (hasSubmenu ? NSAccessibility.Action.showMenu.description : nil))
            if row.kind == .header {
                setAccessibilityValue(title)
            } else if !row.keyEquivalent.accessibilityText.isEmpty {
                setAccessibilityValue(row.keyEquivalent.accessibilityText)
            }
        }

        override func accessibilityFrame() -> NSRect {
            withCurrentMenu(.zero) { $0.menuView?.rowScreenFrame($1) ?? .zero }
        }

        override func isAccessibilitySelected() -> Bool {
            withCurrentMenu(false) { $0.menuView?.selection == $1 }
        }

        override func isAccessibilityExpanded() -> Bool {
            hasSubmenu && withCurrentMenu(false) { $0.menuView?.expandedRow == $1 }
        }

        override func isAccessibilitySelectorAllowed(_ selector: Selector) -> Bool {
            if selector == #selector(accessibilityPerformPress) {
                return isAccessibilityEnabled()
            }
            if selector == #selector(isAccessibilityExpanded) {
                return hasSubmenu
            }
            return super.isAccessibilitySelectorAllowed(selector)
        }

        override func accessibilityPerformPress() -> Bool {
            guard isAccessibilityEnabled() else { return false }
            return withCurrentMenu(false) { menu, index in
                guard let menuView = menu.menuView, menuView.window?.isVisible == true,
                      let activate = menuView.onActivateRow else { return false }
                activate(index)
                return true
            }
        }

        override func accessibilityPerformCancel() -> Bool {
            withCurrentMenu(false) { menu, _ in menu.accessibilityPerformCancel() }
        }

        private func withCurrentMenu<Result: Sendable>(
            _ fallback: Result,
            _ action: @MainActor (AlternativeMenuAccessibilityView, Int) -> Result
        ) -> Result {
            guard Thread.isMainThread else { return fallback }
            let owner = owner
            let index = index
            let identity = ObjectIdentifier(self)
            return MainActor.assumeIsolated {
                guard let menu = owner(), menu.containsRow(at: index, identity: identity) else { return fallback }
                return action(menu, index)
            }
        }
    }
}

/// Embeds the menu's AppKit accessibility tree in SwiftUI without drawing or handling pointer input.
private struct AlternativeMenuAccessibilityHostingView: NSViewRepresentable {
    let menu: AlternativeMenuAccessibilityView

    func makeNSView(context: Context) -> AlternativeMenuAccessibilityView { menu }
    func updateNSView(_ view: AlternativeMenuAccessibilityView, context: Context) {}
}

extension View {
    /// Replaces this view's accessibility elements with the supplied menu tree without changing its layout.
    func alternativeMenuAccessibility(_ menu: AlternativeMenuAccessibilityView) -> some View {
        accessibilityHidden(true)
            .background {
                AlternativeMenuAccessibilityHostingView(menu: menu)
                    .allowsHitTesting(false)
            }
    }
}
