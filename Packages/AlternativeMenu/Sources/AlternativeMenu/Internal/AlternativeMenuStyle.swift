//
//  AlternativeMenuStyle.swift
//  AlternativeMenu
//
//  Copyright © 2015-2026 Clipy Project.
//

import AppKit
import CoreText

/// Constants for matching the appearance and layout of `NSMenu` as closely as possible.
/// The current values are based on the native menu style in macOS 26.
struct AlternativeMenuStyle: Sendable {
    static let current: Self = {
        if #available(macOS 26, *) {
            Self(
                cornerRadius: 12,
                selectionCornerRadius: 7,
                minimumRowHeight: 24
            )
        } else {
            Self(
                cornerRadius: 6,
                selectionCornerRadius: 4,
                minimumRowHeight: 22
            )
        }
    }()

    let padding: CGFloat = 5
    // Minimum inset from screen edges and the area reserved for a bottom Dock.
    let screenEdgeInset: CGFloat = 6
    let cornerRadius: CGFloat
    // The native selection radius is independent of the outer radius and menu padding.
    let selectionCornerRadius: CGFloat
    // Measured minimum item-row height in NSMenu, also used for the scroll arrow area.
    let minimumRowHeight: CGFloat
    // Full separator-row height measured in NSMenu, including space around the line.
    let separatorRowHeight: CGFloat = 11
    // Total vertical allowance for font metrics, title bounds, and images, fitted to native row heights.
    // This is an empirical adjustment, not a measured pair of top and bottom insets.
    let rowContentVerticalPadding: CGFloat = 6
    let rowTextInset: CGFloat = 11
    let iconSpacing: CGFloat = 6
    // Native menus share trailing space between submenu arrows and key equivalents.
    let submenuColumnWidth: CGFloat = 31
    let minimumKeyEquivalentColumnWidth: CGFloat = 32
    // Space beside the measured shortcut, whose width already includes modifier spacing.
    let keyEquivalentGap: CGFloat = 18
    @MainActor
    static let menuFont = NSFont.menuFont(ofSize: 0)
    @MainActor
    static let keyEquivalentFont = CTFontCreateUIFontForLanguage(.menuItemCmdKey, 0, nil).map { $0 as NSFont } ?? menuFont
    @MainActor
    static let headerFont = NSFont.systemFont(ofSize: NSFont.smallSystemFontSize, weight: .semibold)
}
