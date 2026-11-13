//
//  AlternativeMenuKeyEquivalentLabel.swift
//  AlternativeMenu
//
//  Copyright © 2015-2026 Clipy Project.
//

import SwiftUI

/// Uses `NSTextField` because SwiftUI `Text` can omit AppKit's zero-advance
/// function-key ligatures, even when supplied with the native shortcut font.
///
/// Rendering modifiers and the key in one attributed string also keeps their
/// baseline and spacing consistent while preserving the key-specific font
/// supplied by AppKit.
struct AlternativeMenuKeyEquivalentLabel: NSViewRepresentable {
    private let display: AlternativeMenuKeyEquivalent.Display
    private let keyColumnWidth: CGFloat
    private let layoutDirection: LayoutDirection
    private let color: NSColor

    init(display: AlternativeMenuKeyEquivalent.Display, keyColumnWidth: CGFloat, layoutDirection: LayoutDirection, color: NSColor) {
        self.display = display
        self.keyColumnWidth = keyColumnWidth
        self.layoutDirection = layoutDirection
        self.color = color
    }

    func makeNSView(context: Context) -> KeyEquivalentTextField {
        let field = KeyEquivalentTextField(labelWithString: "")
        field.setAccessibilityElement(false)
        return field
    }

    func updateNSView(_ field: KeyEquivalentTextField, context: Context) {
        let title = display.text(sharedKeyColumnWidth: keyColumnWidth, isRightToLeft: layoutDirection == .rightToLeft)
        title.addAttribute(.foregroundColor, value: color, range: NSRange(location: 0, length: title.length))
        field.attributedStringValue = title
    }

    func sizeThatFits(_ proposal: ProposedViewSize, nsView: KeyEquivalentTextField, context: Context) -> CGSize? {
        CGSize(
            width: display.width(sharedKeyColumnWidth: keyColumnWidth),
            height: nsView.intrinsicContentSize.height
        )
    }

    final class KeyEquivalentTextField: NSTextField {
        // Text can extend beyond its native image column; the paragraph compensates for the leading inset.
        override var alignmentRectInsets: NSEdgeInsets {
            let inset = AlternativeMenuStyle.current.keyEquivalentDrawingInset
            return NSEdgeInsets(top: 0, left: inset, bottom: 0, right: inset)
        }
    }
}
