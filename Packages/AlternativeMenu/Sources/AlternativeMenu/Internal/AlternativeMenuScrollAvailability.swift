//
//  AlternativeMenuScrollAvailability.swift
//  AlternativeMenu
//
//  Copyright © 2015-2026 Clipy Project.
//

import Foundation

/// Describes whether a menu can scroll up or down at its current position.
///
/// Derives availability from the content offset and viewport size, and provides
/// the insets reserved for scroll indicators in each available direction.
struct AlternativeMenuScrollAvailability: Equatable {
    private static let scrollThreshold: CGFloat = 1

    let canScrollUp: Bool
    let canScrollDown: Bool
    let topInset: CGFloat
    let bottomInset: CGFloat

    init(offset: CGFloat, contentHeight: CGFloat, viewportHeight: CGFloat, topPadding: CGFloat) {
        self.canScrollUp = offset > Self.scrollThreshold
        self.canScrollDown = offset + viewportHeight < contentHeight - AlternativeMenuStyle.current.padding - Self.scrollThreshold
        // The padding above the viewport already occupies part of the scroll indicator's area.
        self.topInset = canScrollUp ? max(0, AlternativeMenuStyle.current.minimumRowHeight - topPadding) : 0
        self.bottomInset = canScrollDown ? AlternativeMenuStyle.current.minimumRowHeight : 0
    }
}
