//
//  AlternativeMenuView.swift
//  AlternativeMenu
//
//  Copyright © 2015-2026 Clipy Project.
//

import AppKit
import SwiftUI

/// Hosts SwiftUI content while exposing row geometry to the panel.
final class AlternativeMenuView: NSView {
    private let state: AlternativeMenuViewState
    private let minimumWidth: CGFloat
    let menuAccessibilityView = AlternativeMenuAccessibilityView()
    let hasHeader: Bool
    let viewportTopInset: CGFloat

    var onActivateRow: ((Int) -> Void)?
    var onCancelMenu: (() -> Void)?
    var growMenuHeightBy: ((CGFloat) -> CGFloat)?
    var selection: Int? { state.selection }
    var hidesSelection: Bool {
        get { state.hidesSelection }
        set { state.hidesSelection = newValue }
    }
    private(set) var expandedRow: Int?
    var preferredWidth: CGFloat {
        max(minimumWidth, state.contentWidth + AlternativeMenuStyle.padding * 2)
    }
    var preferredHeight: CGFloat {
        viewportTopInset + state.contentHeight
    }

    nonisolated override var isFlipped: Bool { true }

    init(header: AlternativeMenuHeader?, minimumSymbolColumnWidth: CGFloat = 0) {
        self.state = AlternativeMenuViewState(minimumSymbolColumnWidth: minimumSymbolColumnWidth)
        self.hasHeader = header != nil
        self.viewportTopInset = header?.height ?? AlternativeMenuStyle.padding
        self.minimumWidth = header?.minimumWidth ?? 0
        super.init(frame: .zero)
        menuAccessibilityView.menuView = self
        let content = AlternativeMenuRoot(state: state, header: header, menuAccessibilityView: menuAccessibilityView)
        let hostingView = NSHostingView(rootView: content)
        hostingView.sizingOptions = []
        hostingView.frame = bounds
        hostingView.autoresizingMask = [.width, .height]
        addSubview(hostingView)
    }

    @available(*, unavailable)
    required init?(coder: NSCoder) { fatalError("init(coder:) is unavailable") }

    func setRows(
        _ items: [AlternativeMenuItem],
        selectedIndex: Int?,
        keyEquivalent: (Int) -> AlternativeMenuKeyEquivalent.Display? = { _ in nil }
    ) {
        expandedRow = nil
        var accumulatedHeight: CGFloat = 0
        let rows = items.enumerated().map { index, item in
            let row = AlternativeMenuRowData(item: item, verticalOffset: accumulatedHeight, keyEquivalent: keyEquivalent(index))
            accumulatedHeight = row.verticalRange.upperBound
            return row
        }
        state.setRows(rows)
        state.selection = selectedIndex
        state.hidesSelection = false
        menuAccessibilityView.setRows(state.rows)
        if window?.isVisible == true {
            NSAccessibility.post(
                element: menuAccessibilityView,
                notification: .layoutChanged,
                userInfo: [.uiElements: [menuAccessibilityView]]
            )
        }
    }

    func setSelection(_ index: Int?) {
        guard state.selection != index else {
            return
        }
        state.selection = index
        if window?.isVisible == true {
            NSAccessibility.post(element: menuAccessibilityView, notification: .selectedChildrenChanged)
        }
    }

    func setExpandedRow(_ index: Int?) {
        expandedRow = index
    }

    func isInHeader(at windowPoint: NSPoint) -> Bool {
        let point = convert(windowPoint, from: nil)
        return hasHeader && bounds.contains(point) && point.y < viewportTopInset
    }

    func rowIndex(at windowPoint: NSPoint) -> Int? {
        guard let viewportPosition = viewportPosition(at: windowPoint),
            edgeScrollDirection(at: windowPoint) == 0
        else {
            return nil
        }
        let rowPosition = state.offset + viewportPosition
        return state.rows.firstIndex { $0.verticalRange.contains(rowPosition) }
    }

    func rowScreenFrame(_ index: Int) -> NSRect? {
        guard let window, state.rows.indices.contains(index) else {
            return nil
        }
        let verticalRange = state.rows[index].verticalRange
        let availability = state.scrollAvailability(viewportHeight: viewportHeight)
        let top = max(verticalRange.lowerBound - state.offset, availability.topInset)
        let bottom = min(verticalRange.upperBound - state.offset, viewportHeight - availability.bottomInset)
        guard bottom > top else {
            return nil
        }
        let rect = NSRect(
            x: AlternativeMenuStyle.padding,
            y: viewportTopInset + top,
            width: bounds.width - AlternativeMenuStyle.padding * 2,
            height: bottom - top
        )
        return window.convertToScreen(convert(rect, to: nil))
    }

    func edgeScrollDirection(at windowPoint: NSPoint) -> CGFloat {
        guard let viewportPosition = viewportPosition(at: windowPoint) else {
            return 0
        }
        let availability = state.scrollAvailability(viewportHeight: viewportHeight)
        if viewportPosition < availability.topInset {
            return -1
        }
        if viewportPosition >= viewportHeight - availability.bottomInset {
            return 1
        }
        return 0
    }

    @discardableResult
    func scrollMenuItems(by delta: CGFloat) -> Bool {
        scrollMenuItems(to: state.offset + delta)
    }

    func scrollPage(down: Bool) -> Int? {
        let indicatorHeight = AlternativeMenuStyle.scrollIndicatorHeight
        let currentAvailability = state.scrollAvailability(viewportHeight: viewportHeight)
        let pageHeight = max(0, viewportHeight - currentAvailability.topInset - currentAvailability.bottomInset)
        let pageOffset = down ? pageHeight : -pageHeight
        let currentTop = state.offset + currentAvailability.topInset
        let hasAdjacentPage = down ? currentAvailability.canScrollDown : currentAvailability.canScrollUp
        // Native menus target a row one line inside the next page, even if
        // that row cannot be selected. Scroll it into view before returning it.
        let targetPosition = down ? currentTop + pageHeight * 2 - indicatorHeight : currentTop - pageHeight + indicatorHeight
        if hasAdjacentPage, pageHeight > indicatorHeight,
            let target = state.rows.firstIndex(where: { $0.verticalRange.contains(targetPosition) }) {
            if state.rows[target].rowHeight > pageHeight {
                // Revealing the whole row would jump to one edge of it.
                scrollMenuItems(by: pageOffset)
            } else {
                scrollRowToVisible(target)
            }
            return target
        }
        scrollMenuItems(by: pageOffset)
        let availability = state.scrollAvailability(viewportHeight: viewportHeight)
        let (top, bottom) = visibleRowBounds(for: availability)
        guard top < bottom else {
            return nil
        }
        let visible = top..<bottom
        let isVisible: (AlternativeMenuRowData) -> Bool = {
            $0.kind == .item && $0.verticalRange.overlaps(visible)
        }
        let target = down ? state.rows.lastIndex(where: isVisible) : state.rows.firstIndex(where: isVisible)
        if let target {
            scrollRowToVisible(target)
        }
        return target
    }

    func scrollRowToVisible(_ index: Int, alignsTallRowAtBottom: Bool = false) {
        guard state.rows.indices.contains(index) else {
            return
        }
        let availability = state.scrollAvailability(viewportHeight: viewportHeight)
        let verticalRange = state.rows[index].verticalRange
        let (top, bottom) = visibleRowBounds(for: availability)
        if state.rows[index].rowHeight > bottom - top {
            if alignsTallRowAtBottom || verticalRange.upperBound <= top {
                scrollMenuItems(to: verticalRange.upperBound - viewportHeight + availability.bottomInset)
            } else if verticalRange.lowerBound >= bottom {
                scrollMenuItems(to: verticalRange.lowerBound - availability.topInset)
            }
        } else if verticalRange.lowerBound < top {
            scrollMenuItems(to: verticalRange.lowerBound - availability.topInset)
        } else if verticalRange.upperBound > bottom {
            scrollMenuItems(to: verticalRange.upperBound - viewportHeight + availability.bottomInset)
        }
    }

    func resetScrollPosition() {
        scrollMenuItems(to: 0)
    }

    private var viewportHeight: CGFloat {
        max(0, bounds.height - viewportTopInset)
    }

    private func visibleRowBounds(for availability: AlternativeMenuScrollAvailability) -> (top: CGFloat, bottom: CGFloat) {
        let offset = state.offset
        return (offset + availability.topInset, offset + viewportHeight - availability.bottomInset)
    }

    private func viewportPosition(at windowPoint: NSPoint) -> CGFloat? {
        let point = convert(windowPoint, from: nil)
        let yInList = point.y - viewportTopInset
        guard yInList >= 0, yInList < viewportHeight,
            point.x >= AlternativeMenuStyle.padding, point.x < bounds.width - AlternativeMenuStyle.padding
        else {
            return nil
        }
        return yInList
    }

    @discardableResult
    private func scrollMenuItems(to offset: CGFloat) -> Bool {
        guard let scrollView = state.scrollView, let document = scrollView.documentView else {
            return false
        }
        let clip = scrollView.contentView
        let target = max(0, min(offset, state.contentHeight - viewportHeight))
        let distance = target - state.offset
        // A native popup reveals more rows by growing the window before
        // scrolling its document. Growth consumes part of this scroll.
        let growth = distance > 0 ? growMenuHeightBy?(distance) ?? 0 : 0
        let delta = max(0, min(target - growth, state.contentHeight - viewportHeight)) - state.offset
        guard delta != 0 else {
            return growth > 0
        }
        var origin = clip.bounds.origin
        origin.y += document.isFlipped ? delta : -delta
        let targetOrigin = clip.constrainBoundsRect(NSRect(origin: origin, size: clip.bounds.size)).origin
        guard targetOrigin != clip.bounds.origin else {
            return growth > 0
        }
        clip.scroll(to: targetOrigin)
        scrollView.reflectScrolledClipView(clip)
        return true
    }
}
