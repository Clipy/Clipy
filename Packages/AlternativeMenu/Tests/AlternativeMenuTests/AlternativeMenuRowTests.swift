import AppKit
import CoreText
import SwiftUI
import Testing
@testable import AlternativeMenu

@MainActor
struct AlternativeMenuRowTests {
    @Test(arguments: [LayoutDirection.leftToRight, .rightToLeft])
    func alignsRowKeyOriginsAndBaselinesAcrossModifierCountsAndGlyphWidths(_ direction: LayoutDirection) throws {
        let state = rows([
            .action("Title", keyEquivalent: "i", keyEquivalentModifierMask: []) { _ in },
            .action("Title", keyEquivalent: "a") { _ in },
            .action("Title", keyEquivalent: "w", keyEquivalentModifierMask: [.control, .option, .shift, .command]) { _ in },
            .action("Title", keyEquivalent: "\u{f704}", keyEquivalentModifierMask: []) { _ in },
            .action("Title", keyEquivalent: "\u{f70f}") { _ in },
            .action("Title", keyEquivalent: "\u{f717}", keyEquivalentModifierMask: [.shift, .command]) { _ in }
        ])
        let hosts = state.rows.map { row in
            let host = NSHostingView(rootView: AnyView(AlternativeMenuRow(
                row: row, isSelected: false, iconWidth: 0,
                trailingColumnWidth: state.trailingColumnWidth, keyColumnWidth: state.keyColumnWidth
            ).environment(\.layoutDirection, direction)))
            host.frame = NSRect(x: 0, y: 0, width: 240, height: 24)
            host.layoutSubtreeIfNeeded()
            return host
        }
        let origins = try hosts.map { host in
            let field = try onlyTextField(in: host)
            let baseline = field.firstBaselineOffsetFromTop
            return field.convert(NSPoint(
                x: try keyDrawingOffset(in: field),
                y: field.isFlipped ? baseline : field.bounds.height - baseline
            ), to: host)
        }
        let reference = try #require(origins.first)
        let bounds = try #require(hosts.first).bounds
        #expect(direction == .rightToLeft ? reference.x < bounds.midX : reference.x > bounds.midX)
        for origin in origins.dropFirst() {
            // Compare the text origin, not ink bounds: I and W have different side bearings.
            #expect(abs(origin.x - reference.x) <= 0.5)
            #expect(abs(origin.y - reference.y) <= 0.5)
        }
    }

    private func rows(_ items: [AlternativeMenuItem]) -> AlternativeMenuViewState {
        let state = AlternativeMenuViewState()
        let matcher = AlternativeMenuKeyEquivalentMatcher()
        matcher.updateItems(items)
        state.setRows(items.enumerated().map { index, item in
            AlternativeMenuRowData(item: item, verticalOffset: 0, keyEquivalent: matcher.display(at: [index]))
        })
        return state
    }

    private func onlyTextField(in view: NSView) throws -> NSTextField {
        func descendants(_ view: NSView) -> [NSTextField] {
            if let field = view as? NSTextField { return [field] }
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
        return CTLineGetTypographicBounds(line, nil, nil, nil)
            - CTLineGetTypographicBounds(try keyLine(in: field), nil, nil, nil)
    }
}
