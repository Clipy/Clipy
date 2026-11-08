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

    var topInset: CGFloat {
        canScrollUp ? AlternativeMenuStyle.scrollIndicatorHeight : 0
    }
    var bottomInset: CGFloat {
        canScrollDown ? AlternativeMenuStyle.scrollIndicatorHeight : 0
    }

    init(offset: CGFloat, contentHeight: CGFloat, viewportHeight: CGFloat) {
        self.canScrollUp = offset > Self.scrollThreshold
        self.canScrollDown = offset + viewportHeight < contentHeight - Self.scrollThreshold
    }
}
