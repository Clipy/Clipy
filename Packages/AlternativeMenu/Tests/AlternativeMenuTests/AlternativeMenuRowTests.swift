import AppKit
import CoreText
import SwiftUI
import Testing

@testable import AlternativeMenu

@MainActor
struct AlternativeMenuRowTests {
    @Test(arguments: [LayoutDirection.leftToRight, .rightToLeft])
    func alignsSharedKeyColumnsAndBaselinesAcrossModifierCountsAndGlyphWidths(_ direction: LayoutDirection) throws {
        let state = makeState([
            .action("Title", keyEquivalent: "i", keyEquivalentModifierMask: []) { _ in },
            .action("Title", keyEquivalent: "a") { _ in },
            .action("Title", keyEquivalent: "w", keyEquivalentModifierMask: [.control, .option, .shift, .command]) { _ in },
            .action("Title", keyEquivalent: "\u{f704}", keyEquivalentModifierMask: []) { _ in },
            .action("Title", keyEquivalent: "\u{f70f}") { _ in },
            .action("Title", keyEquivalent: "\u{f717}", keyEquivalentModifierMask: [.shift, .command]) { _ in }
        ])
        let hosts = state.rows.map { makeHost(row: $0, state: state, direction: direction) }
        let origins = try hosts.map { host in
            let field = try onlyTextField(in: host)
            // Include the attributed function-key offset when measuring the glyph baseline.
            let keyBaselineOffset = field.attributedStringValue.attribute(.baselineOffset, at: try keyStart(in: field), effectiveRange: nil) as? CGFloat ?? 0
            let baseline = field.firstBaselineOffsetFromTop - keyBaselineOffset
            var anchor = try keyDrawingOffset(in: field)
            if #available(macOS 27, *) {
                let line = try keyLine(in: field)
                let advance = CTLineGetTypographicBounds(line, nil, nil, nil)
                let bounds = CTLineGetBoundsWithOptions(line, .useGlyphPathBounds)
                try #require(!bounds.isNull)
                anchor += (advance > 0 ? advance : ceil(bounds.width)) / 2
            }
            return field.convert(
                NSPoint(
                    x: anchor,
                    y: field.isFlipped ? baseline : field.bounds.height - baseline
                ),
                to: host
            )
        }
        let reference = try #require(origins.first)
        let bounds = try #require(hosts.first).bounds
        #expect(direction == .rightToLeft ? reference.x < bounds.midX : reference.x > bounds.midX)
        for origin in origins.dropFirst() {
            // Native keys share their text origin before macOS 27 and their column center afterward.
            #expect(abs(origin.x - reference.x) <= 0.5)
            #expect(abs(origin.y - reference.y) <= 0.5)
        }
    }

    @Test(arguments: [NSEvent.ModifierFlags(), .command])
    func keepsRightToLeftKeyOriginsStableWhenLongNamesChangeMenuWidth(_ modifiers: NSEvent.ModifierFlags) throws {
        let ordinary = AlternativeMenuItem.action("Title", keyEquivalent: "o", keyEquivalentModifierMask: modifiers) { _ in }
        let space = AlternativeMenuItem.action("Title", keyEquivalent: " ", keyEquivalentModifierMask: modifiers) { _ in }
        let ordinaryOnly = makeState([ordinary])
        let spaceOnly = makeState([space])
        let mixed = makeState([ordinary, space])
        try #require(mixed.trailingColumnWidth > ordinaryOnly.trailingColumnWidth)
        try #require(spaceOnly.keyColumnWidth == 0)

        let ordinaryOrigin = try keyDrawingX(in: makeHost(row: ordinaryOnly.rows[0], state: ordinaryOnly, direction: .rightToLeft))
        let spaceOrigin = try keyDrawingX(in: makeHost(row: spaceOnly.rows[0], state: spaceOnly, direction: .rightToLeft))
        let mixedOrdinaryOrigin = try keyDrawingX(in: makeHost(row: mixed.rows[0], state: mixed, direction: .rightToLeft))
        let mixedSpaceOrigin = try keyDrawingX(in: makeHost(row: mixed.rows[1], state: mixed, direction: .rightToLeft))

        #expect(abs(ordinaryOrigin - mixedOrdinaryOrigin) <= 0.5)
        #expect(abs(spaceOrigin - mixedSpaceOrigin) <= 0.5)
    }

    @Test
    func movesTheRightToLeftKeyColumnByTheNativeModifierWidth() throws {
        let bare = AlternativeMenuItem.action("Title", keyEquivalent: "o", keyEquivalentModifierMask: []) { _ in }
        let modified = AlternativeMenuItem.action("Title", keyEquivalent: "o", keyEquivalentModifierMask: [.option, .shift, .command]) { _ in }
        let bareOnly = makeState([bare])
        let mixed = makeState([bare, modified])
        let before = try keyDrawingX(in: makeHost(row: bareOnly.rows[0], state: bareOnly, direction: .rightToLeft))
        let after = try keyDrawingX(in: makeHost(row: mixed.rows[0], state: mixed, direction: .rightToLeft))
        let modifiedOrigin = try keyDrawingX(in: makeHost(row: mixed.rows[1], state: mixed, direction: .rightToLeft))

        #expect(abs(after - before - mixed.modifierColumnWidth) <= 0.5)
        #expect(abs(after - modifiedOrigin) <= 0.5)
    }

    private func makeHost(row: AlternativeMenuRowData, state: AlternativeMenuViewState, direction: LayoutDirection) -> NSHostingView<AnyView> {
        let host = NSHostingView(
            rootView: AnyView(
                AlternativeMenuRow(
                    row: row,
                    isEmphasized: false,
                    iconWidth: 0,
                    trailingColumnWidth: state.trailingColumnWidth,
                    keyColumnWidth: state.keyColumnWidth,
                    modifierColumnWidth: state.modifierColumnWidth
                ).environment(\.layoutDirection, direction)
            )
        )
        host.frame = NSRect(x: 0, y: 0, width: 240, height: 24)
        host.layoutSubtreeIfNeeded()
        return host
    }

    private func keyDrawingX(in host: NSView) throws -> CGFloat {
        let field = try onlyTextField(in: host)
        return field.convert(NSPoint(x: try keyDrawingOffset(in: field), y: 0), to: host).x
    }

    private func makeState(_ items: [AlternativeMenuItem]) -> AlternativeMenuViewState {
        let state = AlternativeMenuViewState()
        let matcher = AlternativeMenuKeyEquivalentMatcher()
        matcher.updateItems(items)
        state.setRows(
            items.enumerated().map { index, item in
                AlternativeMenuRowData(item: item, verticalOffset: 0, keyEquivalent: matcher.display(at: [index]))
            }
        )
        return state
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
