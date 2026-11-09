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
    // Minimum inset from screen edges and the area reserved for a bottom Dock.
    static let screenEdgeInset: CGFloat = 6
    static let cornerRadius: CGFloat = {
        if #available(macOS 26, *) {
            return 12
        }
        return 6
    }()
    // The native selection radius is independent of the outer radius and menu padding.
    static let selectionCornerRadius: CGFloat = {
        if #available(macOS 26, *) {
            return 7
        }
        return 4
    }()
    // Measured minimum item-row height in NSMenu, also used for the scroll arrow area.
    static let minimumRowHeight: CGFloat = {
        if #available(macOS 26, *) {
            return 24
        }
        return 22
    }()
    // Full separator-row height measured in NSMenu, including space around the line.
    static let separatorRowHeight: CGFloat = 11
    // Total vertical allowance for font metrics, title bounds, and images, fitted to native row heights.
    // This is an empirical adjustment, not a measured pair of top and bottom insets.
    static let rowContentVerticalPadding: CGFloat = 6
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
