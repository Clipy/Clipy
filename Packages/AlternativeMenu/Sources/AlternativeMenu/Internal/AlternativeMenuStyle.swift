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
        if #available(macOS 27, *) {
            return Self(
                cornerRadius: 12,
                selectionCornerRadius: 7,
                minimumRowHeight: 24,
                rowTextInset: 11,
                imagelessRowTextInset: 11,
                centersKeyEquivalents: true,
                centeredModifierLeadingInset: 2,
                keyEquivalentTrailingPadding: 3,
                keyEquivalentRightToLeftOffset: -1,
                longKeyEquivalentPadding: 2,
                longKeyEquivalentLeadingInset: 2,
                submenuArrowTrailingPadding: 3.5,
                submenuArrowRightToLeftTrailingPadding: 3.5,
                headerBaselineOffset: 2,
                imageHorizontalOffset: -1,
                functionKeyBaselineOffset: 1
            )
        } else if #available(macOS 26, *) {
            return Self(
                cornerRadius: 12,
                selectionCornerRadius: 7,
                minimumRowHeight: 24,
                rowTextInset: 11,
                imagelessRowTextInset: 11,
                centersKeyEquivalents: false,
                centeredModifierLeadingInset: 0,
                keyEquivalentTrailingPadding: 0,
                keyEquivalentRightToLeftOffset: -3,
                longKeyEquivalentPadding: 4,
                longKeyEquivalentLeadingInset: 0,
                submenuArrowTrailingPadding: 0.5,
                submenuArrowRightToLeftTrailingPadding: -2.5,
                headerBaselineOffset: 2,
                imageHorizontalOffset: 0,
                functionKeyBaselineOffset: 0
            )
        } else {
            return Self(
                cornerRadius: 6,
                selectionCornerRadius: 4,
                minimumRowHeight: 22,
                rowTextInset: 9,
                imagelessRowTextInset: 9,
                centersKeyEquivalents: false,
                centeredModifierLeadingInset: 0,
                keyEquivalentTrailingPadding: 0,
                keyEquivalentRightToLeftOffset: -1,
                longKeyEquivalentPadding: 8,
                longKeyEquivalentLeadingInset: 0,
                submenuArrowTrailingPadding: 0.5,
                submenuArrowRightToLeftTrailingPadding: -0.5,
                headerBaselineOffset: 1,
                imageHorizontalOffset: 0,
                functionKeyBaselineOffset: 0
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
    let rowTextInset: CGFloat
    // Extra width reserved by native menus for each nonempty attributed title.
    let attributedTitleWidthPadding: CGFloat = 2
    // Before macOS 26, imageless titles begin 14 pt from the menu edge, including padding.
    let imagelessRowTextInset: CGFloat
    let iconSpacing: CGFloat = 6
    // Native menus share trailing space between submenu arrows and key equivalents.
    let submenuColumnWidth: CGFloat = 31
    let minimumKeyEquivalentColumnWidth: CGFloat = 32
    // Space beside the measured shortcut, whose width already includes modifier spacing.
    let keyEquivalentGap: CGFloat = 18
    // Native minimum width adds this margin to the item's own key and modifier image widths.
    let keyEquivalentSizingPadding: CGFloat = 23
    let centersKeyEquivalents: Bool
    // Native modifier images start farther inward than text beside the centered key column.
    let centeredModifierLeadingInset: CGFloat
    let keyEquivalentTrailingPadding: CGFloat
    // Align the RTL label frame with native modifier and key columns, independently of menu sizing.
    let keyEquivalentRightToLeftOffset: CGFloat
    let longKeyEquivalentPadding: CGFloat
    // Native left-aligned key names retain a text inset beside centered key columns.
    let longKeyEquivalentLeadingInset: CGFloat
    let submenuArrowTrailingPadding: CGFloat
    let submenuArrowRightToLeftTrailingPadding: CGFloat
    let headerBaselineOffset: CGFloat
    let imageHorizontalOffset: CGFloat
    let functionKeyBaselineOffset: CGFloat
    let keyEquivalentDrawingInset: CGFloat = 4
    let modifierToKeySpacing: CGFloat = 1.5
    let legacySymbolKeyLeadingInset: CGFloat = 2
    // Older AppKit versions reserve a fixed column for function-key ligatures despite returning zero advance.
    let legacyFunctionKeyColumnWidth: CGFloat = 15
    @MainActor
    static let menuFont = NSFont.menuFont(ofSize: 0)
    @MainActor
    static let keyEquivalentFont = CTFontCreateUIFontForLanguage(.menuItemCmdKey, 0, nil).map { $0 as NSFont } ?? menuFont
    @MainActor
    static let headerFont = NSFont.systemFont(ofSize: NSFont.smallSystemFontSize, weight: .semibold)
}
