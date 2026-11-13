import AppKit
import CoreText
import Testing

@testable import AlternativeMenu

@MainActor
struct AlternativeMenuKeyEquivalentTests {
    struct DisplayExample: Sendable {
        let key: String
        let modifiers: NSEvent.ModifierFlags
        let expected: String
    }

    @Test(arguments: [
        DisplayExample(key: "", modifiers: .command, expected: ""),
        .init(key: "a", modifiers: .command, expected: "⌘A"),
        .init(key: "A", modifiers: .command, expected: "⇧⌘A"),
        .init(key: "ß", modifiers: .command, expected: "⌘ß"),
        .init(key: "ﬃ", modifiers: .command, expected: "⌘ﬃ"),
        .init(key: "a", modifiers: [.command, .shift, .option, .control], expected: "⌃⌥⇧⌘A"),
        .init(key: "\u{f704}", modifiers: [], expected: "F1"),
        .init(key: "\u{f703}", modifiers: .option, expected: "⌥→"),
        .init(key: "\u{08}", modifiers: .command, expected: "⌘⌫"),
        .init(key: "\u{03}", modifiers: [], expected: "⌤"),
        .init(key: "\t", modifiers: .shift, expected: "⇧⇥"),
        .init(key: "\u{19}", modifiers: .shift, expected: "⇧⇤"),
        .init(key: "\r", modifiers: [], expected: "↩"),
        .init(key: "\n", modifiers: [], expected: "↩"),
        .init(key: "\u{1b}", modifiers: [], expected: "⎋"),
        .init(key: "\u{f729}", modifiers: [], expected: "↖"),
        .init(key: "\u{f72b}", modifiers: [], expected: "↘"),
        .init(key: "\u{f72c}", modifiers: [], expected: "⇞"),
        .init(key: "\u{f72d}", modifiers: [], expected: "⇟"),
        .init(key: "\u{7f}", modifiers: [], expected: "⌦"),
        .init(key: "\u{f728}", modifiers: [], expected: "⌦")
    ])
    func displaysKeyEquivalent(_ example: DisplayExample) {
        #expect(AlternativeMenuKeyEquivalent.display(for: example.key, modifiers: example.modifiers).accessibilityText == example.expected)
    }

    @Test
    func separatesVisibleSymbolsFromAccessibleKeyNames() {
        let arrows = [
            ("\u{f700}", "↑", "▲"), ("\u{f701}", "↓", "▼"),
            ("\u{f702}", "←", "◀"), ("\u{f703}", "→", "▶")
        ]
        for (code, key, visualKey) in arrows {
            let arrow = AlternativeMenuKeyEquivalent.display(for: code, modifiers: .option)
            #expect(arrow.accessibilityText == "⌥" + key)
            #expect(arrow.attributedText.string == "⌥ \u{2066}\(visualKey)\u{2069}")
        }
        #expect(AlternativeMenuKeyEquivalent.display(for: "→", modifiers: []).attributedText.string == "\u{2066}→\u{2069}")

        let function = AlternativeMenuKeyEquivalent.display(for: "\u{f70f}", modifiers: .command)
        #expect(function.attributedText.string == "⌘ \u{2066}\u{f861}F12\u{2069}")
        #expect(function.accessibilityText == "⌘F12")
    }

    @Test(arguments: [1, 12, 19, 20, 21, 35])
    func keepsReadableFunctionKeyNamesSeparateFromNativeGlyphs(_ number: Int) throws {
        let scalar = try #require(UnicodeScalar(0xF704 + number - 1))
        let display = AlternativeMenuKeyEquivalent.display(for: String(scalar), modifiers: .command)
        #expect(display.accessibilityText == "⌘F\(number)")
        let expected =
            switch number {
            case 1...9: "\u{f860}F\(number)"
            case 10...20: "\u{f861}F\(number)"
            default: String(scalar)
            }
        #expect(display.attributedText.string == "⌘ \u{2066}\(expected)\u{2069}")
        let glyphRange = (display.attributedText.string as NSString).range(of: expected)
        try #require(glyphRange.location != NSNotFound)
        let font = try #require(display.attributedText.attribute(.font, at: glyphRange.location, effectiveRange: nil) as? NSFont)
        #expect(font.pointSize > 0)
        #expect(display.keyWidth > 0)

        let line = CTLineCreateWithAttributedString(display.attributedText.attributedSubstring(from: glyphRange))
        if number <= 20 {
            #expect(CTLineGetTypographicBounds(line, nil, nil, nil) == 0)
            #expect(!CTLineGetBoundsWithOptions(line, .useGlyphPathBounds).isNull)
        }
    }

    @Test
    func emptyKeyDoesNotReserveAColumn() {
        let display = AlternativeMenuKeyEquivalent.display(for: "", modifiers: .command)

        #expect(display.accessibilityText.isEmpty)
        #expect(display.attributedText.length == 0)
        #expect(display.keyWidth == 0)
        #expect(display.minimumWidth == 0)
        #expect(display.width(sharedKeyColumnWidth: 32) == 0)
    }

    @Test
    func nativeModifierMeasurementOnlyAffectsMenuSizing() {
        let measured = AlternativeMenuKeyEquivalent.Display(
            modifierSymbols: "⌘", keyName: "O", keyGlyphs: "O", keyFont: AlternativeMenuStyle.keyEquivalentFont,
            nativeKeyWidth: 11, nativeModifierWidth: 13, isSymbolKey: false
        )
        let fallback = AlternativeMenuKeyEquivalent.Display(
            modifierSymbols: "⌘", keyName: "O", keyGlyphs: "O", keyFont: AlternativeMenuStyle.keyEquivalentFont,
            nativeKeyWidth: 11, nativeModifierWidth: nil, isSymbolKey: false
        )

        #expect(measured.minimumWidth == 47)
        #expect(measured.attributedText == fallback.attributedText)
        #expect(measured.text(sharedKeyColumnWidth: 32, isRightToLeft: false) == fallback.text(sharedKeyColumnWidth: 32, isRightToLeft: false))
        #expect(measured.width(sharedKeyColumnWidth: 32) == fallback.width(sharedKeyColumnWidth: 32))
        #expect(fallback.minimumWidth > measured.minimumWidth)
    }

    @Test(arguments: [CGFloat.nan, .infinity, -1])
    func invalidNativeMeasurementsFallBackToText(_ width: CGFloat) {
        let fallback = AlternativeMenuKeyEquivalent.Display(
            modifierSymbols: "⌘", keyName: "O", keyGlyphs: "O", keyFont: AlternativeMenuStyle.keyEquivalentFont,
            nativeKeyWidth: nil, nativeModifierWidth: nil, isSymbolKey: false
        )
        let invalid = AlternativeMenuKeyEquivalent.Display(
            modifierSymbols: "⌘", keyName: "O", keyGlyphs: "O", keyFont: AlternativeMenuStyle.keyEquivalentFont,
            nativeKeyWidth: width, nativeModifierWidth: width, isSymbolKey: false
        )

        #expect(invalid == fallback)
        #expect(invalid.minimumWidth.isFinite)
        #expect(invalid.minimumWidth > 0)
    }

    @Test
    func emptyDrawingNameKeepsModifierText() {
        let display = AlternativeMenuKeyEquivalent.Display(
            modifierSymbols: "⌘", keyName: "A", keyGlyphs: "", keyFont: AlternativeMenuStyle.keyEquivalentFont,
            nativeKeyWidth: nil, nativeModifierWidth: nil, isSymbolKey: true
        )

        #expect(display.text(sharedKeyColumnWidth: 32, isRightToLeft: false).string == "⌘ ")
    }

    @Test(arguments: ["\r", "\n"])
    func mirrorsReturnForRightToLeftMenusWithoutChangingItsAccessibleName(_ key: String) throws {
        let display = AlternativeMenuKeyEquivalent.display(for: key, modifiers: .command)
        let left = display.text(sharedKeyColumnWidth: 32, isRightToLeft: false)
        let right = display.text(sharedKeyColumnWidth: 32, isRightToLeft: true)

        #expect(left.string == "⌘ \u{2066}⏎\u{2069}")
        #expect(right.string == "⌘ \u{2066}↪\u{2069}")
        #expect(display.attributedText.string == left.string)
        #expect(display.accessibilityText == "⌘↩")
        let index = (right.string as NSString).range(of: "↪").location
        try #require(index != NSNotFound)
        #expect(left.attribute(.font, at: index, effectiveRange: nil) as? NSFont == right.attribute(.font, at: index, effectiveRange: nil) as? NSFont)
    }

    @Test(arguments: ["\u{01}", "\u{200b}", "\u{feff}"])
    func invisibleKeysReserveOnlyTheirModifiers(_ key: String) {
        let bare = AlternativeMenuKeyEquivalent.display(for: key, modifiers: [])
        let modified = AlternativeMenuKeyEquivalent.display(for: key, modifiers: .command)

        #expect(bare.keyWidth == 0)
        #expect(bare.width(sharedKeyColumnWidth: 0) == 0)
        #expect(modified.keyWidth == 0)
        #expect(modified.width(sharedKeyColumnWidth: 0) > 0)
    }

    @Test(arguments: [NSEvent.ModifierFlags(), .shift, .command, [.shift, .command]])
    func distinguishesTabAndBacktabUsingTheDrawingNames(_ modifiers: NSEvent.ModifierFlags) {
        let tab = AlternativeMenuKeyEquivalent.display(for: "\t", modifiers: modifiers)
        let backtab = AlternativeMenuKeyEquivalent.display(for: "\u{19}", modifiers: modifiers)
        let symbols = (modifiers.contains(.shift) ? "⇧" : "") + (modifiers.contains(.command) ? "⌘" : "")
        let drawingPrefix = symbols.isEmpty ? "" : symbols + " "
        #expect(tab.accessibilityText == symbols + "⇥")
        #expect(backtab.accessibilityText == symbols + "⇤")
        #expect(tab.attributedText.string == "\(drawingPrefix)\u{2066}⇥\u{2069}")
        #expect(backtab.attributedText.string == "\(drawingPrefix)\u{2066}⇤\u{2069}")
    }
}
