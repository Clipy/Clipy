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
    private let color: NSColor

    init(display: AlternativeMenuKeyEquivalent.Display, keyColumnWidth: CGFloat, color: NSColor) {
        self.display = display
        self.keyColumnWidth = keyColumnWidth
        self.color = color
    }

    func makeNSView(context: Context) -> KeyEquivalentTextField {
        let field = KeyEquivalentTextField(labelWithString: "")
        field.setAccessibilityElement(false)
        return field
    }

    func updateNSView(_ field: KeyEquivalentTextField, context: Context) {
        let title = NSMutableAttributedString(attributedString: display.attributedText)
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
        // Leave room beyond the shared columns for AppKit's text inset and zero-advance glyphs.
        override var alignmentRectInsets: NSEdgeInsets { NSEdgeInsets(top: 0, left: 0, bottom: 0, right: 4) }
    }
}
