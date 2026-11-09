//
//  AlternativeMenuPanel.swift
//  AlternativeMenu
//
//  Copyright © 2015-2026 Clipy Project.
//

import AppKit
import SwiftUI

/// Positions one menu window and routes selection and scrolling to its view.
@MainActor
final class AlternativeMenuPanel: NSPanel {
    let menuView: AlternativeMenuView
    let minimumSymbolColumnWidth: CGFloat
    let toolTip = AlternativeMenuToolTip()
    private let acceptsKeyboard: Bool
    private(set) var items: [AlternativeMenuItem] = []
    private var anchorY: CGFloat = 0
    private var opensAbove = false
    private var wheelScrollRemainder: CGFloat = 0

    var onResignKey: (() -> Void)?

    override var canBecomeKey: Bool { acceptsKeyboard }
    override var canBecomeMain: Bool { false }

    override func constrainFrameRect(_ frameRect: NSRect, to screen: NSScreen?) -> NSRect {
        // Placement already reserves the bottom Dock; let the panel cover the menu bar area.
        frameRect
    }

    var selection: Int? { menuView.selection }
    var hasMarkedText: Bool { (firstResponder as? NSTextView)?.hasMarkedText() == true }
    var isHeaderControlFocused: Bool {
        menuView.hasHeader && (firstResponder is NSTextView || firstResponder is NSControl)
    }
    var isRightToLeft: Bool { menuView.userInterfaceLayoutDirection == .rightToLeft }

    init(
        header: AlternativeMenuHeader? = nil,
        minimumSymbolColumnWidth: CGFloat = 0,
        acceptsKeyboard: Bool = true
    ) {
        self.minimumSymbolColumnWidth = minimumSymbolColumnWidth
        self.acceptsKeyboard = acceptsKeyboard
        self.menuView = AlternativeMenuView(header: header, minimumSymbolColumnWidth: minimumSymbolColumnWidth)
        super.init(
            contentRect: .zero,
            styleMask: [.borderless, .nonactivatingPanel],
            backing: .buffered,
            defer: false
        )
        isFloatingPanel = true
        // Setting isFloatingPanel resets the level; restore it above the menu bar.
        level = .popUpMenu
        collectionBehavior = [.auxiliary, .stationary, .moveToActiveSpace, .fullScreenAuxiliary]
        animationBehavior = .none
        isOpaque = false
        backgroundColor = .clear
        hasShadow = true
        hidesOnDeactivate = false
        isMovable = false
        isExcludedFromWindowsMenu = true
        isReleasedWhenClosed = false
        becomesKeyOnlyIfNeeded = false
        acceptsMouseMovedEvents = true
        allowsToolTipsWhenApplicationIsInactive = true
        contentView = menuView
        menuView.growMenuHeightBy = { [weak self] distance in
            self?.growMenuHeight(by: distance) ?? 0
        }
    }

    func open(at point: NSPoint) {
        guard let screen = NSScreen.screens.first(where: { $0.frame.contains(point) }) ?? NSScreen.main else {
            return
        }
        guard menuView.hasHeader || items.contains(where: { !$0.isSeparator }) else {
            return
        }
        anchorY = point.y
        fitMenu(on: screen, openingAt: point)
        menuView.resetScrollPosition()
        makeKeyAndOrderFront(nil)
        layoutAndRevealSelection()
    }

    func openSubmenu(beside parent: AlternativeMenuPanel, rowFrame: NSRect, prefersLeft: Bool) {
        guard let screen = parent.screen else {
            return
        }
        let bounds = menuBounds(on: screen)
        let size = NSSize(
            width: fittedWidth(menuView.preferredWidth, screenWidth: screen.frame.width, availableWidth: bounds.width),
            height: ceil(menuView.preferredHeight)
        )
        let frame = AlternativeMenuSubmenuGeometry.frame(
            preferredSize: size,
            parentFrame: parent.frame,
            rowFrame: rowFrame,
            bounds: bounds,
            prefersLeft: prefersLeft
        )
        guard !frame.isEmpty else {
            return
        }
        setFrame(frame, display: false)
        menuView.resetScrollPosition()
        orderFrontRegardless()
        layoutAndRevealSelection()
    }

    func dismiss() {
        setItems([], highlightsFirstItem: false)
        orderOut(nil)
        // A zero-duration animator update stops an in-flight fade before reuse.
        NSAnimationContext.runAnimationGroup { context in
            context.duration = 0
            animator().alphaValue = 1
        }
        makeFirstResponder(nil)
    }

    func animateDismissal(
        fading panels: [AlternativeMenuPanel],
        blinksSelection: Bool,
        completion: @escaping @MainActor () -> Void
    ) -> Task<Void, Never> {
        Task {
            do {
                try Task.checkCancellation()
                // Confirmation blinks once; cancellation only fades the open menu chain.
                if blinksSelection {
                    menuView.hidesSelection = true
                    try await Task.sleep(for: .milliseconds(67))
                    menuView.hidesSelection = false
                    try await Task.sleep(for: .milliseconds(33))
                }
                await withCheckedContinuation { continuation in
                    NSAnimationContext.runAnimationGroup { context in
                        context.duration = 0.25
                        panels.forEach { $0.animator().alphaValue = 0 }
                    } completionHandler: {
                        continuation.resume()
                    }
                }
                try Task.checkCancellation()
                completion()
            } catch {
                // Immediate dismissal or reopening cancels the animation.
            }
        }
    }

    override func resignKey() {
        super.resignKey()
        onResignKey?()
    }

    func setItems(
        _ items: [AlternativeMenuItem],
        highlightsFirstItem: Bool,
        keyEquivalent: (Int) -> AlternativeMenuKeyEquivalent.Display? = { _ in nil }
    ) {
        toolTip.hide()
        wheelScrollRemainder = 0
        self.items = items
        let selectedIndex = highlightsFirstItem ? items.firstIndex(where: \.isEnabled) : nil
        menuView.setRows(items, selectedIndex: selectedIndex, keyEquivalent: keyEquivalent)
    }

    func updateItems(
        _ items: [AlternativeMenuItem],
        highlightsFirstItem: Bool,
        keyEquivalent: (Int) -> AlternativeMenuKeyEquivalent.Display? = { _ in nil }
    ) {
        setItems(items, highlightsFirstItem: highlightsFirstItem, keyEquivalent: keyEquivalent)
        if let screen {
            fitMenu(on: screen)
        }
        menuView.resetScrollPosition()
        layoutAndRevealSelection()
    }

    func rowIndex(at point: NSPoint) -> Int? {
        menuView.rowIndex(at: convertPoint(fromScreen: point))
    }

    func edgeScrollDirection(at point: NSPoint) -> CGFloat {
        menuView.edgeScrollDirection(at: convertPoint(fromScreen: point))
    }

    func selectRow(_ index: Int?, scroll: Bool = false) {
        let index = index.flatMap { items.indices.contains($0) && items[$0].isEnabled ? $0 : nil }
        menuView.setSelection(index)
        if scroll, let index {
            menuView.scrollRowToVisible(index)
        }
    }

    func moveSelection(by direction: Int) {
        let current = selection
        let isEnabled: (Int) -> Bool = { self.items[$0].isEnabled }
        let adjacent =
            direction > 0
            ? items.indices.dropFirst((current ?? -1) + 1).first(where: isEnabled)
            : items.indices.prefix(current ?? items.count).last(where: isEnabled)
        if let next = adjacent ?? current {
            selectRow(next, scroll: true)
        }
    }

    func scrollWheelByEvent(_ event: NSEvent) -> Bool {
        toolTip.hide()
        let lineHeight = AlternativeMenuStyle.current.minimumRowHeight
        let distance = -event.scrollingDeltaY
        let movement: CGFloat
        if event.phase.isEmpty && event.momentumPhase.isEmpty {
            wheelScrollRemainder = 0
            movement = distance * lineHeight
        } else {
            // NSMenu accumulates a trackpad gesture and moves in whole menu lines.
            if event.phase == .began || event.phase == .cancelled {
                wheelScrollRemainder = 0
            }
            wheelScrollRemainder += distance
            movement = (wheelScrollRemainder / lineHeight).rounded(.towardZero) * lineHeight
            wheelScrollRemainder -= movement
        }

        let pointer = convertPoint(toScreen: event.locationInWindow)
        guard movement != 0, menuView.scrollMenuItems(by: movement) else {
            return false
        }
        selectRow(rowIndex(at: pointer))
        return true
    }

    func updateToolTip(for index: Int?) {
        guard let index, let text = items[index].toolTip, !text.isEmpty,
            let rowFrame = menuView.rowScreenFrame(index)
        else {
            toolTip.hide(resetDelay: false)
            return
        }
        toolTip.schedule(text, beside: self, rowFrame: rowFrame)
    }
}

private extension AlternativeMenuPanel {
    func fittedWidth(_ preferredWidth: CGFloat, screenWidth: CGFloat, availableWidth: CGFloat) -> CGFloat {
        // NSMenu caps the width at 60% of the display, in points.
        let limit = screenWidth * 0.6
        let nearestPoint = limit.rounded()
        // Match NSMenu's 1/8 pt tolerance: snap to the nearest integer when less than
        // 0.125 pt away (e.g. 613.95 becomes 614); otherwise round the limit down.
        let maximumWidth = abs(limit - nearestPoint) < 0.125 ? nearestPoint : floor(limit)
        return min(ceil(preferredWidth), maximumWidth, availableWidth)
    }

    func layoutAndRevealSelection() {
        menuView.layoutSubtreeIfNeeded()
        if let selection {
            menuView.scrollRowToVisible(selection)
        }
    }

    func fitMenu(on screen: NSScreen, openingAt point: NSPoint? = nil) {
        let bounds = menuBounds(on: screen)
        let previousFrame = frame
        let anchorBottom = min(max(anchorY, bounds.minY), bounds.maxY)
        let leadingEdge: CGFloat
        let preferredWidth: CGFloat
        let top: CGFloat
        var bottom: CGFloat
        if let point {
            opensAbove = false
            leadingEdge = point.x
            preferredWidth = menuView.preferredWidth
            // NSMenu anchors its outer top edge at the point when the first item is a hidden separator.
            let topPadding = !menuView.hasHeader && items.first?.isSeparator == true ? 0 : AlternativeMenuStyle.current.padding
            top = min(max(point.y + topPadding, bounds.minY), bounds.maxY)
            bottom = anchorBottom
        } else {
            leadingEdge = isRightToLeft ? previousFrame.maxX : previousFrame.minX
            preferredWidth = max(previousFrame.width, menuView.preferredWidth)
            top = min(previousFrame.maxY, bounds.maxY)
            bottom = max(previousFrame.minY, bounds.minY)
        }

        let preferredHeight = ceil(menuView.preferredHeight)
        let spaceBelow = top - bounds.minY
        // Prefer below when the content fits or at least four rows can be seen.
        if !opensAbove, spaceBelow < min(preferredHeight, menuView.viewportTopInset + AlternativeMenuStyle.current.minimumRowHeight * 4),
            bounds.maxY - anchorBottom > spaceBelow {
            opensAbove = true
            bottom = anchorBottom
        }
        // Keep the edge nearest the pointer fixed as the menu items change.
        let availableHeight = opensAbove ? bounds.maxY - bottom : spaceBelow
        let height = min(preferredHeight, availableHeight)
        let width = fittedWidth(
            preferredWidth,
            screenWidth: screen.frame.width,
            availableWidth: bounds.width
        )
        let originX = isRightToLeft ? leadingEdge - width : leadingEdge
        let newFrame = NSRect(
            x: min(max(originX, bounds.minX), bounds.maxX - width),
            y: opensAbove ? bottom : top - height,
            width: width,
            height: height
        )
        if point != nil || previousFrame != newFrame {
            setFrame(newFrame, display: point == nil && isVisible)
        }
    }

    func growMenuHeight(by distance: CGFloat) -> CGFloat {
        guard distance > 0, let screen else {
            return 0
        }
        let oldFrame = frame
        let bounds = menuBounds(on: screen)
        let needed = min(distance, ceil(menuView.preferredHeight) - oldFrame.height)
        guard needed > 0 else {
            return 0
        }
        // The bottom stays in place while the popup can grow upward. A
        // bottom-anchored popup can reclaim the gap below once it hits the top.
        let above = min(needed, max(0, bounds.maxY - oldFrame.maxY))
        let below = min(needed - above, max(0, oldFrame.minY - bounds.minY))
        guard above + below > 0 else {
            return 0
        }
        toolTip.hide()
        setFrame(
            NSRect(
                x: oldFrame.minX,
                y: oldFrame.minY - below,
                width: oldFrame.width,
                height: oldFrame.height + above + below
            ),
            display: true
        )
        menuView.layoutSubtreeIfNeeded()
        return frame.height - oldFrame.height
    }

    func menuBounds(on screen: NSScreen) -> NSRect {
        var bounds = screen.frame
        // NSMenu avoids the bottom Dock, but can overlap a side Dock and the menu bar.
        bounds.origin.y = screen.visibleFrame.minY
        bounds.size.height = screen.frame.maxY - bounds.minY
        let inset = AlternativeMenuStyle.current.screenEdgeInset
        return bounds.insetBy(dx: inset, dy: inset)
    }
}
