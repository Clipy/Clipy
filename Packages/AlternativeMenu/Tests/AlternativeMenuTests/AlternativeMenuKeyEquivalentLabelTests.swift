import AppKit
import CoreText
import SwiftUI
import Testing

@testable import AlternativeMenu

@MainActor
struct AlternativeMenuKeyEquivalentLabelTests {
    @Test(arguments: ["a", "\u{f704}", "\u{f70f}", "\u{f717}", " "], [NSEvent.ModifierFlags(), [.control, .option, .shift, .command]])
    func rendersModifiersAndNativeKeyGlyphsInOneTextField(_ key: String, modifiers: NSEvent.ModifierFlags) throws {
        let display = AlternativeMenuKeyEquivalent.display(for: key, modifiers: modifiers)
        try #require(display.attributedText.length > 0)
        let host = hosting(display)
        let field = try onlyTextField(in: host)
        let title = field.attributedStringValue

        // Attributed paragraphs control drawing independently of the field's fallback settings.
        title.enumerateAttribute(.paragraphStyle, in: NSRange(location: 0, length: title.length)) { value, _, _ in
            let paragraph = value as? NSParagraphStyle
            #expect(paragraph?.alignment == .left)
            #expect(paragraph?.baseWritingDirection == .leftToRight)
            #expect(paragraph?.lineBreakMode == .byClipping)
        }
        #expect(title.string.hasPrefix(modifiers.isEmpty ? "\u{2066}" : "⌃⌥⇧⌘ "))
        #expect(title.string == display.attributedText.string)
        #expect(!title.string.contains("\t"))
        title.enumerateAttribute(.attachment, in: NSRange(location: 0, length: title.length)) { attachment, _, _ in
            #expect(attachment == nil)
        }
        let start = try keyStart(in: field)
        let expectedFont = try #require(display.attributedText.attribute(.font, at: start, effectiveRange: nil) as? NSFont)
        #expect(title.attribute(.font, at: start, effectiveRange: nil) as? NSFont == expectedFont)
        let bounds = CTLineGetBoundsWithOptions(try keyLine(in: field), .useGlyphPathBounds)
        try #require(!bounds.isNull)
        #expect(field.bounds.width - (try keyDrawingOffset(in: field)) >= bounds.maxX)
    }

    @Test(arguments: ["\t", "\u{19}", "\r", "\u{1b}", "\u{08}", "\u{f728}", "\u{f746}"], [CGFloat(0), 32])
    func keepsBareSpecialKeysInsideTheDrawingBounds(_ key: String, keyColumnWidth: CGFloat) throws {
        let display = AlternativeMenuKeyEquivalent.display(for: key, modifiers: [])
        for direction in [LayoutDirection.leftToRight, .rightToLeft] {
            let host = hosting(display, keyColumnWidth: keyColumnWidth, layoutDirection: direction)
            let field = try onlyTextField(in: host)
            let bounds = CTLineGetBoundsWithOptions(try keyLine(in: field), .useGlyphPathBounds)
            try #require(!bounds.isNull)
            let origin = try keyDrawingOffset(in: field)

            #expect(origin + bounds.minX >= field.bounds.minX)
            #expect(origin + bounds.maxX <= field.bounds.maxX)
        }
    }

    @Test(arguments: ["\t", "\r", "\u{f703}"], [LayoutDirection.leftToRight, .rightToLeft])
    func keepsSpecialKeyOriginsStableAcrossModifierCounts(_ key: String, direction: LayoutDirection) throws {
        let modifiers: [NSEvent.ModifierFlags] = [[], .command, [.shift, .command], [.control, .option]]
        let hosts = modifiers.map {
            hosting(AlternativeMenuKeyEquivalent.display(for: key, modifiers: $0), keyColumnWidth: 32, layoutDirection: direction)
        }
        let origins = try hosts.map { host in
            let field = try onlyTextField(in: host)
            return field.convert(NSPoint(x: try keyDrawingOffset(in: field), y: 0), to: host).x
        }
        let reference = try #require(origins.first)

        for origin in origins.dropFirst() {
            #expect(abs(origin - reference) <= 0.5)
        }
    }

    @Test(arguments: ["\t", "\r", "\u{f703}"])
    func alignsImageKeyCentersWithTheMenuDirection(_ key: String) throws {
        let display = AlternativeMenuKeyEquivalent.display(for: key, modifiers: .command)
        let columnWidth: CGFloat = 32
        let left = hosting(display, keyColumnWidth: columnWidth, layoutDirection: .leftToRight)
        let right = hosting(display, keyColumnWidth: columnWidth, layoutDirection: .rightToLeft)
        let leftField = try onlyTextField(in: left)
        let rightField = try onlyTextField(in: right)
        let leftCenter = try keyDrawingOffset(in: leftField) + CTLineGetTypographicBounds(keyLine(in: leftField), nil, nil, nil) / 2
        let rightCenter = try keyDrawingOffset(in: rightField) + CTLineGetTypographicBounds(keyLine(in: rightField), nil, nil, nil) / 2

        #expect(abs(rightCenter - leftCenter - (columnWidth - display.keyWidth)) <= 0.001)
    }

    @Test
    func updatesTheWholeShortcutColorWithoutChangingTheNativeGlyphsOrFont() throws {
        let display = AlternativeMenuKeyEquivalent.display(for: " ", modifiers: .command)
        let host = hosting(display, color: .tertiaryLabelColor)
        let original = try onlyTextField(in: host).attributedStringValue

        host.rootView = content(display, keyColumnWidth: 0, layoutDirection: .leftToRight, color: .selectedMenuItemTextColor)
        host.layoutSubtreeIfNeeded()
        let field = try onlyTextField(in: host)
        let updated = field.attributedStringValue

        #expect(updated.string == original.string)
        updated.enumerateAttribute(.foregroundColor, in: NSRange(location: 0, length: updated.length)) { color, _, _ in
            #expect(color as? NSColor == .selectedMenuItemTextColor)
        }
        let start = try keyStart(in: field)
        let expectedFont = try #require(original.attribute(.font, at: start, effectiveRange: nil) as? NSFont)
        #expect(updated.attribute(.font, at: start, effectiveRange: nil) as? NSFont == expectedFont)
        let sharedTitle = display.attributedText
        sharedTitle.enumerateAttribute(.foregroundColor, in: NSRange(location: 0, length: sharedTitle.length)) { color, _, _ in
            #expect(color == nil)
        }
    }

    private func hosting(
        _ display: AlternativeMenuKeyEquivalent.Display,
        keyColumnWidth: CGFloat = 0,
        layoutDirection: LayoutDirection = .leftToRight,
        color: NSColor = .tertiaryLabelColor
    ) -> NSHostingView<AnyView> {
        let host = NSHostingView(rootView: content(display, keyColumnWidth: keyColumnWidth, layoutDirection: layoutDirection, color: color))
        host.frame = NSRect(x: 0, y: 0, width: 240, height: 24)
        host.layoutSubtreeIfNeeded()
        return host
    }

    private func content(
        _ display: AlternativeMenuKeyEquivalent.Display, keyColumnWidth: CGFloat, layoutDirection: LayoutDirection, color: NSColor
    ) -> AnyView {
        AnyView(
            AlternativeMenuKeyEquivalentLabel(display: display, keyColumnWidth: keyColumnWidth, layoutDirection: layoutDirection, color: color)
                .frame(width: 240, height: 24, alignment: .trailing)
        )
    }

    private func onlyTextField(in view: NSView) throws -> NSTextField {
        func descendants(_ view: NSView) -> [NSTextField] {
            if let field = view as? NSTextField {
                return [field]
            }
            return view.subviews.flatMap(descendants)
        }
        let fields = descendants(view)
        try #require(fields.count == 1)
        return try #require(fields.first)
    }

    private func keyStart(in field: NSTextField) throws -> Int {
        let range = (field.stringValue as NSString).range(of: "\u{2066}")
        try #require(range.location != NSNotFound)
        return range.upperBound
    }

    private func keyLine(in field: NSTextField) throws -> CTLine {
        let start = try keyStart(in: field)
        let end = (field.stringValue as NSString).range(of: "\u{2069}", options: .backwards).location
        try #require(end != NSNotFound && end >= start)
        let text = field.attributedStringValue.attributedSubstring(from: NSRange(location: start, length: end - start))
        return CTLineCreateWithAttributedString(text)
    }

    private func keyDrawingOffset(in field: NSTextField) throws -> CGFloat {
        // Caret offsets include half of the preceding kern; glyph drawing starts at the modifier advance.
        let line = CTLineCreateWithAttributedString(field.attributedStringValue)
        let paragraph = try #require(
            field.attributedStringValue.attribute(.paragraphStyle, at: 0, effectiveRange: nil) as? NSParagraphStyle
        )
        let cell = try #require(field.cell)
        return cell.drawingRect(forBounds: field.bounds).minX + paragraph.firstLineHeadIndent
            + CTLineGetTypographicBounds(line, nil, nil, nil)
            - CTLineGetTypographicBounds(try keyLine(in: field), nil, nil, nil)
    }
}
