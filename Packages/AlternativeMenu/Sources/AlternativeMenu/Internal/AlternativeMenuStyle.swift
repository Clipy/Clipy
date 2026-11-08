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
enum AlternativeMenuStyle {
    static let padding: CGFloat = 5
    // Minimum inset between the menu window and physical screen edges.
    static let screenEdgeInset: CGFloat = 6
    static let cornerRadius: CGFloat = 12
    // Minimum item-row height measured in NSMenu on macOS 26.
    static let minimumRowHeight: CGFloat = 24
    // Full separator-row height measured in NSMenu, including space around the line.
    static let separatorRowHeight: CGFloat = 11
    // Total vertical allowance for image and measured title bounds, fitted to native row heights.
    // This is an empirical adjustment, not a measured pair of top and bottom insets.
    static let rowContentVerticalPadding: CGFloat = 6
    // Added to rounded font line metrics to approximate native row heights.
    // This empirical correction can still differ by about 1 pt for some fonts or mixed-font titles.
    static let rowFontHeightAdjustment: CGFloat = 7
    // The native scroll target occupies one normal menu row.
    static let scrollIndicatorHeight: CGFloat = 24
    static let rowTextInset: CGFloat = 11
    static let iconSpacing: CGFloat = 6
    // Native menus share trailing space between submenu arrows and key equivalents.
    static let submenuColumnWidth: CGFloat = 31
    static let minimumKeyEquivalentColumnWidth: CGFloat = 32
    // Space beside the measured shortcut, whose width already includes modifier spacing.
    static let keyEquivalentGap: CGFloat = 18
    @MainActor
    static let menuFont = NSFont.menuFont(ofSize: 0)
    @MainActor
    static let keyEquivalentFont = CTFontCreateUIFontForLanguage(.menuItemCmdKey, 0, nil).map { $0 as NSFont } ?? menuFont
    @MainActor
    static let headerFont = NSFont.systemFont(ofSize: NSFont.smallSystemFontSize, weight: .semibold)
}
